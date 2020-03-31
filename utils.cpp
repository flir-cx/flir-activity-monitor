#include "utils.hpp"
#include <iostream>
#include <fstream>

template<typename t>
t get_value_from_file(const std::string &filename, t failed_value) {
    std::ifstream f(filename);
    if (!f.good()) {
        return failed_value;
    }
    t value = failed_value;
    f >> value;

    return value;
}


timestamp_t get_timestamp() {
    using std::chrono::duration_cast;
    using std::chrono::seconds;
    using std::chrono::steady_clock;
    const auto now = std::chrono::steady_clock::now();
    const auto duration = duration_cast<seconds>(now.time_since_epoch());

    timestamp_t time = duration.count();

    return time;
}

bool get_charger_online(const settings_t &settings) {
    std::string charger_file = "/sys/class/power_supply/";
    charger_file += settings.charger_name;
    charger_file += "/online";
    int online = get_value_from_file(charger_file, -1);

    return online == 1;
}
