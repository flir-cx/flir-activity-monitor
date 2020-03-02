#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <poll.h>
#include <string.h>

#include "log.hpp"
#include "state_handler.hpp"
#include "settings_handler.hpp"
#include "input_listener.hpp"
#include "utils.hpp"


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
    /* Enable breaking and signalling to loop */
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGHUP);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    const int signal_fd = signalfd(-1, &sigset, 0);

    logger_setup(log_type_t::SYSLOG, log_level_t::INFO);

    SettingsHandler settings_handler;

    if (!settings_handler.startDbusThread()) {
        return EXIT_FAILURE;
    }

    state_t current_state = state_t::ACTIVE;

    int count = 0;
    bool stop_application = false;

    do {

        if (!settings_handler.generateSettings()) {
            return EXIT_FAILURE;
        }
        const auto settings = settings_handler.getSettings();

        if (start_input_listener(settings) < 0) {
            return EXIT_FAILURE;
        }
        do {
            struct pollfd fd = {.fd = signal_fd, .events = POLL_IN, .revents = 0};
            int r = poll(&fd, 1, 1000);

            // Got signal
            if (r > 0) {
                struct signalfd_siginfo fdsi;
                read(fd.fd, &fdsi, sizeof(struct signalfd_siginfo));

                switch(fdsi.ssi_signo) {
                case SIGINT:
                case SIGTERM:
                case SIGQUIT:
                    stop_application = true;
                    break;
                default:
                    // Will break inner loop and reinitialize.
                    break;
                }
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

        stop_input_listener();
    } while (!stop_application);


    LOG_INFO("Shutting down application.");


    return 0;
}
