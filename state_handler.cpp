#include "state_handler.hpp"

#include <chrono>
#include <iostream>

state_t get_new_state(const state_t current_state, const settings_t &settings, const activity_log_t &activity_log, const timepoint_t &now) {

    if (settings.battery_monitor_mode == battery_monitor_mode_t::BOTH ||
        settings.battery_monitor_mode == battery_monitor_mode_t::CURRENT) {
        if (activity_log.battery_current < settings.battery_current_level) {
            std::cerr << "Battery current level to low, transition to shutdown: "
                << activity_log.battery_current
                << std::endl;
            return state_t::SHUTDOWN;
        }
    }

    if (settings.battery_monitor_mode == battery_monitor_mode_t::BOTH ||
        settings.battery_monitor_mode == battery_monitor_mode_t::PERCENTAGE) {
        if (activity_log.battery_percentage < settings.battery_percentage_limit) {
            std::cerr << "Battery percentage level to low, transition to shutdown: "
                << activity_log.battery_percentage
                << std::endl;
            return state_t::SHUTDOWN;
        }
    }

    if (now > (activity_log.last_input + settings.inactivity_limit_seconds)) {
        return state_t::SLEEP;
    }

    return state_t::ACTIVE;
}
