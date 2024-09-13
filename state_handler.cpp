#include "state_handler.hpp"

#include <chrono>
#include <iostream>

#include "log.hpp"


state_t get_new_state(const state_t current_state,
        const settings_t &settings,
        const status_t &status,
        const timestamp_t &now)
{
  if (status.force_poweroff_state)
        return state_t::SHUTDOWN;

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

    if (settings.sleep_enabled)
    {
        bool bLownet = (status.net.max_traffic_last_period < settings.net_activity_limit);
        int inputLimit = status.input.charger_online ? settings.inactive_on_charger_limit : settings.inactive_on_battery_limit;
        bool bNoCable = (now > (status.cable.lastconnected_time  + settings.no_cable_secs));

        if (bLownet && inputLimit > 0 && bNoCable)
        {
            const timestamp_t alert_seconds = 30;
            const timestamp_t inputTimeout = status.input.event_time + static_cast<timestamp_t>(inputLimit);

            if (now > inputTimeout)
            {
                LOG_NOTICE("System is inactive: (inactivity time: %d seconds, net activity: %f, charger: %d), will perform sleep command.",
                        (now - status.input.event_time),
                        status.net.max_traffic_last_period,
                        status.input.charger_online);
                return state_t::SLEEP;
            }
            else if (now + alert_seconds >= inputTimeout)
            {
                return state_t::ALERT;
            }
        }
    }
    return state_t::ACTIVE;
}
