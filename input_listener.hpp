#pragma once
#include "types.hpp"

int start_input_listener(const settings_t &settings);
int stop_input_listener();

timestamp_t get_last_input_event_time();
void reset_last_input_event_time();


