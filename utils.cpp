#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <string>

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
    std::string charger_file = "/etc/sysfs-links/";
    charger_file += settings.charger_name;
    charger_file += "/online";
    int online = get_value_from_file(charger_file, -1);

    return online == 1;
}
