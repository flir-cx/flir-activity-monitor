#pragma once
#include "types.hpp"

typedef enum class state {
    ACTIVE,
    SLEEP,
    SHUTDOWN,
} state_t;

state_t get_new_state(const state_t current_state,
        const settings_t &settings,
        const activity_log_t &activity_log,
        const timestamp_t &now);
