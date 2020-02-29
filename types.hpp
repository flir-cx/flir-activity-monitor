#pragma once
#include <chrono>
#include <vector>
#include <string>

using timestamp_t = uint32_t;

typedef struct {
    timestamp_t event_time;
    bool charger_online;
} input_event_data_t;

typedef struct {
   input_event_data_t last_input;
   double battery_voltage;
   double battery_percentage;
   double net_traffic_max;
} activity_log_t;

typedef enum class batter_monitor_mode {
    NONE,
    VOLTAGE,
    PERCENTAGE,
    BOTH,
} battery_monitor_mode_t;

typedef struct {
    battery_monitor_mode_t battery_monitor_mode;
    double battery_voltage_limit;
    double battery_percentage_limit;
    double net_activity_limit;
    std::vector<std::string> input_event_devices;
    std::vector<std::string> net_devices;
    int inactivity_limit_seconds;
    std::string sleep_system_cmd;
    std::string shutdown_system_cmd;
    std::string charger_name;
    std::string battery_name;
} settings_t;
