#include "gtest/gtest.h"
#include <chrono>

#include "../state_handler.hpp"
#include "../utils.hpp"


TEST(StateHandler, TransitionToSleep) {
    const auto current_state = state_t::ACTIVE;
    const auto now = get_timestamp();

    settings_t settings = {};
    activity_log_t activity_log = {};
    activity_log.last_input.event_time = now;
    activity_log.last_input.charger_online = 0;

    const auto active_state = get_new_state(current_state, settings, activity_log, now);
    const auto sleep_state = get_new_state(current_state, settings, activity_log, now + 3);

    EXPECT_EQ(active_state, state_t::ACTIVE);
    EXPECT_EQ(sleep_state, state_t::SLEEP);
}




