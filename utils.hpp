#pragma once
#include "types.hpp"
#include <string>
#include <iostream>
#include <fstream>

namespace {
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
}

timestamp_t get_timestamp();

bool get_charger_online(const settings_t &settings);
