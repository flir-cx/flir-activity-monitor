#include "gtest/gtest.h"
#include <chrono>

#include "../state_handler.hpp"
#include "../utils.hpp"


TEST(StateHandler, TransitionToSleep) {
    const auto current_state = state_t::ACTIVE;
    const auto now = get_timestamp();

    settings_t settings = {};
    settings.sleep_enabled = true;
    settings.inactive_on_battery_limit = 60;
    settings.net_activity_limit = 100;
    status_t status = {};
    status.input.event_time = now;
    status.input.charger_online = 0;

    const auto active_state = get_new_state(current_state, settings, status, now);
    const auto sleep_state = get_new_state(current_state, settings, status, now + 61);

    EXPECT_EQ(active_state, state_t::ACTIVE);
    EXPECT_EQ(sleep_state, state_t::SLEEP);
}




