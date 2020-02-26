#pragma once
#include "types.hpp"

int start_input_listener(const settings_t &settings);
int stop_input_listener();

input_event_data_t get_last_input_event_data();
void reset_last_input_event_data(const settings_t &settings);


