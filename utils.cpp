#include "utils.hpp"
#include <iostream>

timepoint_t get_timestamp() {
    using std::chrono::duration_cast;
    using std::chrono::seconds;
    using std::chrono::steady_clock;
    const auto now = std::chrono::steady_clock::now();
    const auto duration = duration_cast<seconds>(now.time_since_epoch());

    timepoint_t time = duration.count();

    return time;
}

