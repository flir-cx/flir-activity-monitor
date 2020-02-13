#include "state_handler.hpp"

#include <chrono>
#include <iostream>

#include "log.h"

state_t get_new_state(const state_t current_state, const settings_t &settings, const activity_log_t &activity_log, const timestamp_t &now) {

    if (settings.battery_monitor_mode == battery_monitor_mode_t::BOTH ||
        settings.battery_monitor_mode == battery_monitor_mode_t::VOLTAGE) {
        if (activity_log.battery_voltage < settings.battery_voltage_limit) {
            LOG_WARNING("Battery voltage level too low: %.2f V, will perform shutdown command.",
                    activity_log.battery_voltage);
            return state_t::SHUTDOWN;
        }
    }

    if (settings.battery_monitor_mode == battery_monitor_mode_t::BOTH ||
        settings.battery_monitor_mode == battery_monitor_mode_t::PERCENTAGE) {
        if (activity_log.battery_percentage < settings.battery_percentage_limit) {
            LOG_WARNING("Battery percentage level too low: %.2f %%, will perform shutdown command. ",
                activity_log.battery_percentage);
            return state_t::SHUTDOWN;
        }
    }

    if (now > (activity_log.last_input + settings.inactivity_limit_seconds)) {
        LOG_NOTICE("System is inactive: (inactivity time: %d seconds, net activity: %f), will perform sleep command.",
                (now - activity_log.last_input),
                activity_log.net_traffic_max);
        return state_t::SLEEP;
    }

    return state_t::ACTIVE;
}
