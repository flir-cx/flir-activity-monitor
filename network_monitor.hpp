#pragma once

#include "types.hpp"

bool start_network_monitor(const settings_t &settings);
bool stop_network_monitor();

double get_max_net_activity();
