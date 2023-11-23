#include "settings_handler.hpp"
#include "utils.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <systemd/sd-bus.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <linux/input.h>

#include "log.hpp"

namespace {

static int method_set_on_battery_idle_limit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int32_t idle_time;
    std::stringstream ss;
    auto settings_handler = reinterpret_cast<SettingsHandler *>(userdata);

    /* Read the parameters */
    int r = sd_bus_message_read(m, "i", &idle_time);
    if (r < 0) {
        LOG_ERROR("Failed to parse parameters: %s", strerror(-r));
        return r;
    }
    LOG_DEBUG("DBUS: Got on battery idle time: %d", idle_time);
    ss << idle_time;
    settings_handler->addDbusSetting(settings_field::INACT_ON_BAT_LIMIT, ss.str());
    // Trigger rereading of settings
    kill(getpid(), SIGHUP);

    /* Reply with the response */
    return sd_bus_reply_method_return(m, nullptr);
}

static int method_get_on_battery_idle_limit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    auto settings_handler = reinterpret_cast<SettingsHandler *>(userdata);
    const auto settings = settings_handler->getSettings();

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", settings.inactive_on_battery_limit);
}

static int method_set_on_charger_idle_limit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int32_t idle_time;
    std::stringstream ss;
    auto settings_handler = reinterpret_cast<SettingsHandler *>(userdata);

    /* Read the parameters */
    int r = sd_bus_message_read(m, "i", &idle_time);
    if (r < 0) {
        LOG_ERROR("Failed to parse parameters: %s", strerror(-r));
        return r;
    }
    LOG_DEBUG("DBUS: Got on charger idle time: %d", idle_time);
    ss << idle_time;
    settings_handler->addDbusSetting(settings_field::INACT_ON_CHARGER_LIMIT, ss.str());
    // Trigger rereading of settings
    kill(getpid(), SIGHUP);

    /* Reply with the response */
    return sd_bus_reply_method_return(m, nullptr);
}

static int method_get_on_charger_idle_limit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    auto settings_handler = reinterpret_cast<SettingsHandler *>(userdata);
    const auto settings = settings_handler->getSettings();

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", settings.inactive_on_charger_limit);
}

static int method_set_sleep_enabled(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int32_t enabled;
    auto settings_handler = reinterpret_cast<SettingsHandler *>(userdata);

    /* Read the parameters */
    int r = sd_bus_message_read(m, "b", &enabled);
    if (r < 0) {
        LOG_ERROR("Failed to parse parameters: %s", strerror(-r));
        return r;
    }
    LOG_DEBUG("DBUS: Got Sleep enabled: %d", enabled);
    settings_handler->addDbusSetting(settings_field::ENABLED_SLEEP, enabled?"true":"false");
    // Trigger rereading of settings
    kill(getpid(), SIGHUP);

    /* Reply with the response */
    return sd_bus_reply_method_return(m, nullptr);
}

static int method_get_sleep_enabled(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    auto settings_handler = reinterpret_cast<SettingsHandler *>(userdata);
    const auto settings = settings_handler->getSettings();

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "b", settings.sleep_enabled?1:0);
}

static const sd_bus_vtable settings_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("SetOnBatteryTimeToSleep", "i", nullptr, method_set_on_battery_idle_limit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetOnBatteryTimeToSleep", nullptr, "i", method_get_on_battery_idle_limit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetOnACTimeToSleep", "i", nullptr, method_set_on_charger_idle_limit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetOnACTimeToSleep", nullptr, "i", method_get_on_charger_idle_limit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetSleepEnabled", "b", nullptr, method_set_sleep_enabled, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetSleepEnabled", nullptr, "b", method_get_sleep_enabled, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};
};

SettingsHandler::SettingsHandler()
: mDefaultSettings{}
, mSettings{}
, mAbortFD(-1)
{
    mDefaultSettings.input_event_devices = {};
    mDefaultSettings.pollonly_event_devices = {};
    mDefaultSettings.inactive_on_battery_limit = 0;
    mDefaultSettings.inactive_on_charger_limit = 0;
    mDefaultSettings.battery_voltage_limit = 3.2;
    mDefaultSettings.battery_capacity_limit = 5;
    mDefaultSettings.battery_monitor_mode = battery_monitor_mode_t::VOLTAGE;
    mDefaultSettings.net_activity_limit = 100;
    mDefaultSettings.net_devices = {
        "wlan0",
        "usb0",
        "p2p0",
    };
    mDefaultSettings.sleep_system_cmd = "systemctl suspend";
    mDefaultSettings.shutdown_flirapp_cmd = "systemctl stop flirapp";
    mDefaultSettings.shutdown_system_cmd = "systemctl poweroff";
    mDefaultSettings.charger_name = "pmic_charger";
    mDefaultSettings.battery_name = "battery";
    mDefaultSettings.sleep_enabled = true;
    // Values defined in the respective camera model battery whitepapers
    mDefaultSettings.battery_voltage_limits = {
        {"-evco", 2.7},
        {"-leco", 2.5},
        {"-ec201", 3.2},
        {"-ec302", 3.2},
    };

    mSettings = mDefaultSettings;
}

SettingsHandler::~SettingsHandler()
{
    if (mAbortFD >= 0) {
        uint64_t v = 1;
        write(mAbortFD, &v, sizeof(v));
        if (mDbusThread.joinable()) {
            mDbusThread.join();
        }
        close(mAbortFD);
    }
}


settings_t
SettingsHandler::getSettings() {
    LOG_DEBUG("Getting settings");
    std::lock_guard<std::mutex> l(mMutex);
    return mSettings;
}


bool
SettingsHandler::startDbusThread() {
    sd_bus_slot *slot = NULL;
    sd_bus *bus = NULL;
    int r;

    r = sd_bus_open_system(&bus);
    if (r < 0) {
        LOG_ERROR("settings: Failed to connect to system bus: %s", strerror(-r));
        sd_bus_unref(bus);
        return false;
    }

    /* Install the object */
    r = sd_bus_add_object_vtable(bus,
            &slot,
            "/com/flir/activitymonitor",  /* object path */
            "com.flir.activitymonitor",   /* interface name */
            settings_vtable,
            this);

    if (r < 0) {
        LOG_ERROR("settings: Failed to issue method call: %s", strerror(-r));
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return false;
    }

    /* Take a well-known service name so that clients can find us */
    r = sd_bus_request_name(bus, "com.flir.activitymonitor", 0);
    if (r < 0) {
        LOG_ERROR("settings: Failed to acquire service name: %s", strerror(-r));
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return false;
    }

    int epollfd = epoll_create1(0);
    struct epoll_event ev;
    mAbortFD = eventfd(0, 0);
    ev.events = EPOLLIN;
    ev.data.fd = mAbortFD;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, mAbortFD, &ev) == -1) {
        LOG_ERROR("settings: epoll_ctl: sd_bus fd: '%s' (%d)", strerror(errno), errno);
        return false;
    }
    int bus_fd = sd_bus_get_fd(bus);
    ev.events = EPOLLIN;
    ev.data.fd = bus_fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, bus_fd, &ev) == -1) {
        LOG_ERROR("settings: epoll_ctl: sd_bus fd: '%s' (%d)", strerror(errno), errno);
        return false;
    }
    //uint64_t wait_usec;
    //sd_bus_get_timeout(bus, &wait_usec);
    //int wait = (wait_usec == uint64_t(-1))?-1:(wait_usec * 999)/1000;

    mDbusThread = std::thread([bus, slot, abortfd = mAbortFD, epollfd]()
    {
        bool stop_thread = false;
        for (;;) {
            struct epoll_event ep_events[2];
            int nfds = epoll_wait(epollfd, ep_events, 2, -1);
            if (nfds == -1) {
                if (errno != EINTR) {
                    LOG_ERROR("settings: epoll_wait: '%s' (%d)", strerror(errno), errno);
                }
                continue;
            }
            for (int n = 0; n < nfds; ++n) {
                if (ep_events[n].data.fd == abortfd) {
                    stop_thread = true;
                    break;
                }
            }
            if (stop_thread) {
                break;
            }

            /* Process requests */
            int r = 0;
            while((r = sd_bus_process(bus, NULL)) > 0);

            if (r < 0) {
                LOG_ERROR("settings: Failed to process bus: %s", strerror(-r));
            }
        }
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        if (epollfd >= 0) {
            close(epollfd);
        }
    });

    return true;
}

bool
SettingsHandler::generateSettings()
{
    LOG_DEBUG("Generating settings");
    std::lock_guard<std::mutex> l(mMutex);
    setBatteryVoltageLimit();
    setInputEventDevices();
    for (const auto &f :mDbusSettings) {
        switch (f.first) {
//            case settings_field::BAT_MONITOR_MODE:
//            case settings_field::BAT_VOLTAGE_LIMIT:
//            case settings_field::BAT_PERCENTAGE_LIMIT:
//            case settings_field::NET_DEVICES:
//            case settings_field::NET_ACTIVITY_LIMIT:
//            case settings_field::INPUT_DEVICES:
            case settings_field::INACT_ON_BAT_LIMIT:
            {
                std::stringstream ss(f.second);
                int limit = -1;
                ss >> limit;
                mSettings.inactive_on_battery_limit = limit;
                LOG_INFO("Applying on battery limit from dbus: '%s', %d",
                        f.second.c_str(), mSettings.inactive_on_battery_limit);
            }
            break;

            case settings_field::INACT_ON_CHARGER_LIMIT:
            {
                std::stringstream ss(f.second);
                int limit = -1;
                ss >> limit;
                mSettings.inactive_on_charger_limit = limit;
                LOG_INFO("Applying on charger limit from dbus: '%s', %d",
                        f.second.c_str(), mSettings.inactive_on_charger_limit);
            }
            break;

//            case settings_field::NAME_BATTERY:
//            case settings_field::NAME_CHARGER:
//            case settings_field::CMD_SLEEP:
//            case settings_field::CMD_SHUTDOWN:
            case settings_field::ENABLED_SLEEP:
            {
                auto enabled = f.second;
                std::transform(enabled.begin(), enabled.end(), enabled.begin(),
                    [](unsigned char c){ return std::tolower(c); });
                mSettings.sleep_enabled = (enabled == "true");
                LOG_INFO("Applying enabled sleep setting from dbus: '%s', %d",
                        f.second.c_str(), mSettings.sleep_enabled);
            }
            break;

            default:
                LOG_ERROR("Generate settings, settings field not handled.");
        }
    }

    return true;
}

void
SettingsHandler::addDbusSetting(settings_field field, const std::string &content)
{
    LOG_DEBUG("ADDING DBUS SETTING: %s", content.c_str());
    std::lock_guard<std::mutex> l(mMutex);
    mDbusSettings[field] = content;
}

void
SettingsHandler::setBatteryVoltageLimit()
{
    // Set battery voltage limit depending on the camera model.
    std::string compatibility_file = "/proc/device-tree/compatible";
    std::string compatibility = "unknown";
    compatibility = get_value_from_file(compatibility_file, compatibility);

    for (const auto& n : mSettings.battery_voltage_limits)
    {
        std::size_t found = compatibility.find(n.first);
        if (found != std::string::npos)
        {
            mSettings.battery_voltage_limit = n.second;

            /* If ec302 mainboard revision is above 4,
             * we can lower shutdown voltage to improve
             * battery life. */
            if (n.first.find("ec302") != std::string::npos)
            {
				FILE* fd;
				char rsp[4] = { 0 };
				int mainboard_revision;

				fd = popen("hexdump -s84 -n3 /sys/bus/i2c/devices/1-0057/eeprom -e '3/1 \"%c\"'", "r");
				if (fd == NULL) 
				{
					LOG_ERROR("Unable to open process");
					break;
				}

				if (fgets(rsp, 3, fd) == NULL)
				{
					LOG_ERROR("Failed to read from process");
					pclose(fd);
					break;
				}

				mainboard_revision = atoi(rsp);
				if (mainboard_revision >= 4)
					mSettings.battery_voltage_limit = 3.0;

				pclose(fd);
            }
        }
    }
    LOG_INFO("Setting battery voltage limit: %f", mSettings.battery_voltage_limit);
}

// Helper function to setInputEventDevices(); lookup entries in dir
// probe name of event device and return list that matches pattern in driver name
static std::vector<std::string>
findInputEventDeviceNameMatch(const std::string &dir,
                              const std::string &acceptpattern)
{
    const std::filesystem::path dir_path{dir};
    std::vector<std::string> event_dev_paths;
    char buf[256];
    int fd;
    int rval;
    std::string cppbuf;

    if (acceptpattern.length() == 0)
        // no pattern
        return event_dev_paths;

    try { // Catch error in case dir does not exist
        for (const auto& dir_entry : std::filesystem::directory_iterator{dir_path}) {
            std::string path = dir_entry.path().string();
            // Return paths containing acceptpattern (empty pattern ok) and file exists
            if (!dir_entry.exists() || dir_entry.is_directory() ||
                dir_entry.is_symlink()) {
                continue;
            }

            fd = open(path.c_str(), O_RDONLY);
            if (fd < 0) {
                LOG_INFO("could not open %s", path.c_str());
                continue;
            }
            // get device name out of input device
            rval = ioctl(fd, EVIOCGNAME(sizeof(buf)), buf);
            close(fd);
            if (rval < 0) {
                // unexpected error in ioctl - skip this node
                LOG_INFO("ioctl error for %d", path.c_str());
                continue;
            }
            cppbuf = buf;  // convert to cpp string
            LOG_INFO("Found input event device %s, name: %s", path.c_str(), cppbuf.c_str());
            if (cppbuf.find(acceptpattern) ==  std::string::npos)
                // not matching acceptpattern
                continue;

            LOG_INFO("Accepted input event match device %s, name: %s", path.c_str(), buf);
            event_dev_paths.push_back(path);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Lookup error for event devices, %s", e.what());
    }

    return event_dev_paths;
}

// Helper function to setInputEventDevices(); lookup entries in dir, match entries using (absolute) prefix
static std::vector<std::string> findInputEventDevicePaths(const std::string& dir, const std::string& acceptpattern)
{
    const std::filesystem::path dir_path{dir};
    std::vector<std::string> event_dev_paths;

    try { // Catch error in case dir does not exist
        for (const auto& dir_entry : std::filesystem::directory_iterator{dir_path}) {
            std::string path = dir_entry.path().string();
            // Return paths containing acceptpattern (empty pattern ok) and file exists
            if (acceptpattern.length() > 0 &&
                path.find(acceptpattern) == std::string::npos) {
                continue;
            }
            else if (!dir_entry.exists() || dir_entry.is_directory()) {
                continue;
            }

            LOG_INFO("Found input event device %s", path.c_str());
            event_dev_paths.push_back(path);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Lookup error for event devices, %s", e.what());
    }

    return event_dev_paths;
}

void
SettingsHandler::setInputEventDevices()
{
    std::vector<std::string> touch_devices;
    // Find out all keyboard devices in /dev/input/by-path/*,
    // which will be monitored

    mSettings.input_event_devices =
        findInputEventDevicePaths("/dev/input/by-path", "key");

    // Add touchscreen device to monitored input devices
    // this event device is not always present in "by-path" (hw dependent),
    // definitely not named "touch" in "by-path"
    // always a symlink "touchscreen0" directly below /dev/input/ 
    touch_devices = findInputEventDevicePaths("/dev/input", "touch");
    for (const auto e : touch_devices) {
        mSettings.input_event_devices.push_back(e);
    }

    // For compatibility, create a pollonly device list
    mSettings.pollonly_event_devices =
        findInputEventDeviceNameMatch("/dev/input", "accel");
}
