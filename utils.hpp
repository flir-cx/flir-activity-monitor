#pragma once
#include "types.hpp"

timestamp_t get_timestamp();

double get_battery_voltage(const settings_t &settings);
double get_battery_percentage(const settings_t &settings);
bool get_charger_online(const settings_t &settings);
double get_max_net_traffic(const settings_t settings);
