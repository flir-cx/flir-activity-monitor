#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <poll.h>
#include <string.h>

#include "log.hpp"
#include "state_handler.hpp"
#include "input_listener.hpp"
#include "utils.hpp"

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

activity_log_t get_activity_log(const settings_t &settings) {
    activity_log_t activity = {
        .last_input = get_last_input_event_data(),
        .battery_voltage = get_battery_voltage(settings),
        .battery_percentage = get_battery_percentage(settings),
        .net_traffic_max = get_max_net_traffic(settings),
    };
    return activity;
}

void reset_activity_log(const settings_t &settings) {
    reset_last_input_event_data(settings);
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
            // returning from suspend
            reset_activity_log(settings);
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
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    const int signal_fd = signalfd(-1, &sigset, 0);
    const auto settings = get_settings();

    logger_setup(log_type_t::SYSLOG, log_level_t::INFO);

    state_t current_state = state_t::ACTIVE;
    start_input_listener(settings);

    int count = 0;

    do {
        struct pollfd fd = {.fd = signal_fd, .events = POLL_IN, .revents = 0};
        int r = poll(&fd, 1, 1000);

        // Got signal to stop
        if (r > 0) {
            break;
        }

        if (r < 0) {
            LOG_ERROR("Main thread poll failed: '%s' (%d).", strerror(errno), errno);
            break;
        }

        const auto activity = get_activity_log(settings);
        const auto now = get_timestamp();
        const auto new_state = get_new_state(current_state,
                settings,
                activity,
                now);

        if (new_state != current_state) {
            handle_transition(settings, current_state, new_state);
            current_state = new_state;
        }

        if ((count++ % 60) == 0) {
            LOG_DEBUG("Charger: %s, Idle: %d s, v: %.2f, p: %f, n: %f",
                    activity.last_input.charger_online ? "ONLINE": "OFFLINE",
                    now - activity.last_input.event_time,
                    activity.battery_voltage,
                    activity.battery_percentage,
                    activity.net_traffic_max);
        }
    } while (true);


    LOG_INFO("Shutting down application.");

    stop_input_listener();

    return 0;
}
