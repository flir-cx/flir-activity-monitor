#pragma once
#include <chrono>
#include <vector>
#include <string>

using timestamp_t = uint64_t;

typedef struct {
   timestamp_t last_input;
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
    std::vector<std::string> input_event_devices;
    std::vector<std::string> net_devices;
    int inactivity_limit_seconds;
    std::string sleep_system_cmd;
    std::string shutdown_system_cmd;
} settings_t;
