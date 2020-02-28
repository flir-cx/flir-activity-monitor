#include "settings_handler.hpp"

#include <thread>

#include <systemd/sd-bus.h>
#include <string.h>
#include <errno.h>

#include "log.hpp"

namespace {
    std::thread dbus_thread;
}

settings_t get_settings() {
    settings_t settings = {};
    settings.input_event_devices = {
        "/dev/input/event0",
        "/dev/input/event1",
        "/dev/input/event2",
        "/dev/input/event3",
        "/dev/input/event4",
    };
    settings.inactivity_limit_seconds = 60;
    settings.battery_voltage_limit = 3.2;
    settings.battery_percentage_limit = 5;
    settings.battery_monitor_mode = battery_monitor_mode_t::VOLTAGE;
    settings.net_devices = {
        "wlan0",
    };
    settings.sleep_system_cmd = "systemctl suspend";
    settings.shutdown_system_cmd = "systemctl poweroff";
    settings.charger_name = "pf1550-charger";

    return settings;
}


static int method_set_idle_limit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    uint32_t idle_time;
    int32_t ret;

    /* Read the parameters */
    int r = sd_bus_message_read(m, "u", &idle_time);
    if (r < 0) {
        LOG_ERROR("Failed to parse parameters: %s", strerror(-r));
        return r;
    }

    LOG_INFO("New idle time: %d", idle_time);

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", ret);
}

static const sd_bus_vtable settings_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("SetIdleTimeToSleep", "u", "i", method_set_idle_limit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

int settings_dbus_start_listener() {
    sd_bus_slot *slot = NULL;
    sd_bus *bus = NULL;
    int r;

    r = sd_bus_open_system(&bus);
    if (r < 0) {
        LOG_ERROR("Failed to connect to system bus: %s", strerror(-r));
        sd_bus_unref(bus);
        return r;
    }

    /* Install the object */
    r = sd_bus_add_object_vtable(bus,
            &slot,
            "/com/flir/activitymonitor",  /* object path */
            "com.flir.activitymonitor",   /* interface name */
            settings_vtable,
            NULL);

    if (r < 0) {
        LOG_ERROR("Failed to issue method call: %s", strerror(-r));
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return r;
    }

    /* Take a well-known service name so that clients can find us */
    r = sd_bus_request_name(bus, "com.flir.activitymonitor", 0);
    if (r < 0) {
        LOG_ERROR("Failed to acquire service name: %s", strerror(-r));
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
        return r;
    }


    dbus_thread = std::thread([bus, slot]() {
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

    return 0;
}
