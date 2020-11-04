#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <poll.h>
#include <string.h>

#include "log.hpp"
#include "state_handler.hpp"
#include "settings_handler.hpp"
#include "input_monitor.hpp"
#include "network_monitor.hpp"
#include "battery_monitor.hpp"
#include "utils.hpp"

status_t get_status(InputMonitor &input, NetworkMonitor &net, BatteryMonitor &bat) {
    status_t status {
        .input = input.getStatus(),
        .net = net.getStatus(),
        .bat = bat.getStatus(),
    };

    return status;
}

bool handle_transition( const settings_t &settings,
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
            return true;
        }
    }

    if (new_state == state_t::SHUTDOWN) {
        if (!settings.shutdown_system_cmd.empty()) {
            LOG_INFO("Shutting down system using: '%s'",
                    settings.shutdown_system_cmd.c_str());
            system(settings.shutdown_system_cmd.c_str());
            return true;
        }
    }

    return false;
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
        LOG_ERROR("Failed to start Dbus thread.");
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

        InputMonitor input_mon(settings);
        if (!input_mon.start()) {
            LOG_ERROR("Failed to start input monitor.");
            return EXIT_FAILURE;
        }

        NetworkMonitor net_mon(settings);
        if (!net_mon.start()) {
            LOG_ERROR("Failed to start network monitor.");
            return EXIT_FAILURE;
        }

        // Rolling window of 10 samples taken 3 seconds a part
        BatteryMonitor bat_mon(settings, 10, 3000);
        if (!bat_mon.start()) {
            LOG_ERROR("Failed to start battery monitor.");
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

            const auto status = get_status(input_mon, net_mon, bat_mon);
            const auto now = get_timestamp();
            const auto new_state = get_new_state(current_state,
                                                 settings,
                                                 status,
                                                 now);

            if (new_state != current_state) {
                if (new_state == state_t::SHUTDOWN) {
                    bat_mon.printData();
                    logger_stat("low-battery-shutdown");
                    usleep(100000); // Sleep to let log messages have time to print
                }
                else if (new_state == state_t::SLEEP) {
                    logger_stat("auto-suspend");
                }
                const bool should_reset = handle_transition(settings, current_state, new_state);
                current_state = new_state;
                if (should_reset) {
                    input_mon.reset();
                    net_mon.reset();
                    bat_mon.reset();
                }
            }
        } while (true);
    } while (!stop_application);


    LOG_INFO("Shutting down application.");


    return 0;
}
