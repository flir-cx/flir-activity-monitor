#include <signal.h>
#include <unistd.h>

#include "log.h"
#include "state_handler.hpp"
#include "input_listener.hpp"
#include "utils.hpp"

//logging

/* signal handling */
static sig_atomic_t abort_signal = 0;
static void signal_handler(int sig) {
    abort_signal = 1;
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
    settings.battery_current_level = 3.2;
    settings.battery_percentage_limit = 5;
    settings.battery_monitor_mode = battery_monitor_mode_t::CURRENT;
    settings.net_devices = {
        "wlan0",
    };
    settings.sleep_system_cmd = "systemctl suspend";
    settings.shutdown_system_cmd = "systemctl poweroff";

    return settings;
}

activity_log_t get_activity() {
    activity_log_t activity = {
        .last_input = get_last_input_event_time(),
        .battery_current = 4.3,
        .battery_percentage = 80,
        .net_traffic_max = 1000,
    };

    activity.last_input = get_last_input_event_time();

    return activity;
}

int handle_transition( const settings_t &settings,
                       const state_t &old_state,
                       const state_t &new_state ) {
    if (new_state == old_state) {
        return 0;
    }

    if (new_state == state_t::SLEEP) {
        if (!settings.sleep_system_cmd.empty()) {
            LOG_INFO("Putting system to sleep using: '%s'",
                    settings.sleep_system_cmd.c_str());
            system(settings.sleep_system_cmd.c_str());
        }
    }

    if (new_state == state_t::SHUTDOWN) {
        if (!settings.shutdown_system_cmd.empty()) {
            system(settings.shutdown_system_cmd.c_str());
        }
    }

    return 0;
}


int main(int argc, char *argv[]) {
    /* Enable breaking loop */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    const auto settings = get_settings();

    state_t current_state = state_t::ACTIVE;
    start_input_listener(settings);
    do {
        sleep(1);
        const auto activity = get_activity();
        const auto now = get_timestamp();
        const auto new_state = get_new_state(current_state,
                                             settings,
                                             activity,
                                             now);

        if (new_state != current_state) {
            handle_transition(settings, current_state, new_state);
            current_state = new_state;
        }
    } while (!abort_signal);


    LOG_INFO("Shutting down application.");

    stop_input_listener();

    return 0;
}
