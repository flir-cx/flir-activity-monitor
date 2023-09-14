#pragma once
#include <chrono>
#include <vector>
#include <string>
#include <map>

using timestamp_t = uint32_t;

typedef struct {
    timestamp_t event_time;
    bool charger_online;
} input_status_t;

typedef struct {
    double max_traffic_last_period;
} network_status_t;

typedef struct {
    bool valid;
    bool voltage_below_limit;
    bool capacity_below_limit;
} battery_status_t;

typedef struct {
    input_status_t input;
    network_status_t net;
    battery_status_t bat;
    bool force_poweroff_state;
} status_t;


typedef enum class batter_monitor_mode {
    NONE,
    VOLTAGE,
    PERCENTAGE,
    BOTH,
} battery_monitor_mode_t;

typedef struct {
    battery_monitor_mode_t battery_monitor_mode;
    double battery_voltage_limit;
    double battery_capacity_limit;
    double net_activity_limit;
    std::vector<std::string> input_event_devices;
    std::vector<std::string> net_devices;
    int inactive_on_battery_limit;
    int inactive_on_charger_limit;
    bool sleep_enabled;
    std::string sleep_system_cmd;
    std::string shutdown_flirapp_cmd;
    std::string shutdown_system_cmd;
    std::string charger_name;
    std::string battery_name;
    std::map<std::string, double> battery_voltage_limits;
} settings_t;
