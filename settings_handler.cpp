#include "settings_handler.hpp"

#include <sstream>

#include <systemd/sd-bus.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

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
, mPollFD(-1)
, mAbortFD(-1)
{
    mDefaultSettings.input_event_devices = {
        "/dev/input/event0",
        "/dev/input/event1",
        "/dev/input/event2",
        "/dev/input/event3",
        "/dev/input/event4",
    };
    mDefaultSettings.inactive_on_battery_limit = 0;
    mDefaultSettings.inactive_on_charger_limit = 0;
    mDefaultSettings.battery_voltage_limit = 3.2;
    mDefaultSettings.battery_percentage_limit = 5;
    mDefaultSettings.battery_monitor_mode = battery_monitor_mode_t::VOLTAGE;
    mDefaultSettings.net_activity_limit = 100;
    mDefaultSettings.net_devices = {
        "wlan0",
        "usb0",
        "p2p0",
    };
    mDefaultSettings.sleep_system_cmd = "systemctl suspend";
    mDefaultSettings.shutdown_system_cmd = "systemctl poweroff";
    mDefaultSettings.charger_name = "pf1550-charger";
    mDefaultSettings.battery_name = "battery";
    mDefaultSettings.sleep_enabled = true;

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
        LOG_ERROR("Failed to connect to system bus: %s", strerror(-r));
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
        LOG_ERROR("Failed to issue method call: %s", strerror(-r));
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return false;
    }

    /* Take a well-known service name so that clients can find us */
    r = sd_bus_request_name(bus, "com.flir.activitymonitor", 0);
    if (r < 0) {
        LOG_ERROR("Failed to acquire service name: %s", strerror(-r));
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return false;
    }

    mPollFD = epoll_create1(0);
    struct epoll_event ev;
    mAbortFD = eventfd(0, 0);
    ev.events = EPOLLIN;
    ev.data.fd = mAbortFD;
    if (epoll_ctl(mPollFD, EPOLL_CTL_ADD, mAbortFD, &ev) == -1) {
        LOG_ERROR("epoll_ctl: sd_bus fd: '%s' (%d)", strerror(errno), errno);
        return false;
    }
    int bus_fd = sd_bus_get_fd(bus);
    ev.events = EPOLLIN;
    ev.data.fd = bus_fd;
    if (epoll_ctl(mPollFD, EPOLL_CTL_ADD, bus_fd, &ev) == -1) {
        LOG_ERROR("epoll_ctl: sd_bus fd: '%s' (%d)", strerror(errno), errno);
        return false;
    }
    //uint64_t wait_usec;
    //sd_bus_get_timeout(bus, &wait_usec);
    //int wait = (wait_usec == uint64_t(-1))?-1:(wait_usec * 999)/1000;

    mDbusThread = std::thread([bus, slot, abortfd = mAbortFD, epollfd = mPollFD]()
    {
        bool stop_thread = false;
        for (;;) {
            struct epoll_event ep_events[2];
            int nfds = epoll_wait(epollfd, ep_events, 2, -1);
            if (nfds == -1) {
                LOG_ERROR("epoll_wait: '%s' (%d)", strerror(errno), errno);
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
                LOG_ERROR("Failed to process bus: %s", strerror(-r));
            }
        }
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
    });

    return true;
}

bool
SettingsHandler::generateSettings()
{
    LOG_DEBUG("Generating settings");
    std::lock_guard<std::mutex> l(mMutex);
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
            }
            break;

            case settings_field::INACT_ON_CHARGER_LIMIT:
            {
                std::stringstream ss(f.second);
                int limit = -1;
                ss >> limit;
                mSettings.inactive_on_charger_limit = limit;
            }
            break;

//            case settings_field::NAME_BATTERY:
//            case settings_field::NAME_CHARGER:
//            case settings_field::CMD_SLEEP:
//            case settings_field::CMD_SHUTDOWN:
            case settings_field::ENABLED_SLEEP:
            {
                std::stringstream ss(f.second);
                bool enabled = false;
                ss >> enabled;
                mSettings.sleep_enabled = enabled;
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
