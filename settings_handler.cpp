#include "settings_handler.hpp"

#include <sstream>

#include <systemd/sd-bus.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

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
    const auto settings_handler = reinterpret_cast<SettingsHandler *>(userdata);
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
    const auto settings_handler = reinterpret_cast<SettingsHandler *>(userdata);
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
    const auto settings_handler = reinterpret_cast<SettingsHandler *>(userdata);
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
{
}

SettingsHandler::~SettingsHandler()
{
}


settings_t
SettingsHandler::getSettings() const {
    LOG_INFO("Getting settings");
    settings_t settings = {};
    settings.input_event_devices = {
        "/dev/input/event0",
        "/dev/input/event1",
        "/dev/input/event2",
        "/dev/input/event3",
        "/dev/input/event4",
    };
    settings.inactive_on_battery_limit = 60;
    settings.inactive_on_charger_limit = 60*20;
    settings.battery_voltage_limit = 3.2;
    settings.battery_percentage_limit = 5;
    settings.battery_monitor_mode = battery_monitor_mode_t::VOLTAGE;
    settings.net_devices = {
        "wlan0",
    };
    settings.sleep_system_cmd = "systemctl suspend";
    settings.shutdown_system_cmd = "systemctl poweroff";
    settings.charger_name = "AC";
    settings.battery_name = "BAT0";
    settings.sleep_enabled = true;

    return settings;
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


    mDbusThread = std::thread([bus, slot]() {
        for (;;) {
            /* Process requests */
            int r = sd_bus_process(bus, NULL);
            if (r < 0) {
                LOG_ERROR("Failed to process bus: %s", strerror(-r));
            }
            if (r > 0) /* we processed a request, try to process another one, right-away */
                continue;

            /* Wait for the next request to process */
            r = sd_bus_wait(bus, (uint64_t) -1);
            if (r < 0) {
                LOG_ERROR("Failed to wait on bus: %s", strerror(-r));
                continue;
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
    LOG_INFO("Generating settings");
    return true;
}

void
SettingsHandler::addDbusSetting(settings_field field, const std::string &content)
{
    LOG_INFO("ADDING DBUS SETTING: %s", content.c_str());
    mDbusSettings[field] = content;
}
