#include "state_handler.hpp"

#include <chrono>
#include <iostream>

#include "log.hpp"


state_t get_new_state(const state_t current_state,
        const settings_t &settings,
        const status_t &status,
        const timestamp_t &now) {

    if (settings.battery_monitor_mode == battery_monitor_mode_t::BOTH ||
        settings.battery_monitor_mode == battery_monitor_mode_t::VOLTAGE) {
        if (status.bat.valid && status.bat.voltage_below_limit) {
            LOG_WARNING("Battery voltage level lower than: %.2f V, will perform shutdown command.",
                    settings.battery_voltage_limit);
            return state_t::SHUTDOWN;
        }
    }

    if (settings.battery_monitor_mode == battery_monitor_mode_t::BOTH ||
        settings.battery_monitor_mode == battery_monitor_mode_t::PERCENTAGE) {
        if (status.bat.valid && status.bat.capacity_below_limit) {
            LOG_WARNING("Battery capacity level lower than: %.2f %%, will perform shutdown command. ",
                settings.battery_capacity_limit);
            return state_t::SHUTDOWN;
        }
    }

    if (settings.sleep_enabled && status.net.max_traffic_last_period < settings.net_activity_limit &&
            ((!status.input.charger_online && settings.inactive_on_battery_limit > 0 &&
             now > (status.input.event_time + settings.inactive_on_battery_limit)) ||
             (status.input.charger_online && settings.inactive_on_charger_limit > 0 &&
              now > (status.input.event_time + settings.inactive_on_charger_limit)))) {
        LOG_NOTICE("System is inactive: (inactivity time: %d seconds, net activity: %f, charger: %d), will perform sleep command.",
                (now - status.input.event_time),
                status.net.max_traffic_last_period,
                status.input.charger_online);
        return state_t::SLEEP;
    }

    return state_t::ACTIVE;
}
