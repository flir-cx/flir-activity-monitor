#pragma once

#include <thread>
#include <mutex>
#include "types.hpp"

class CableMonitor {
public:
    CableMonitor(const settings_t settings);
    ~CableMonitor();
    cable_status_t getStatus();
    void printData();
    
private:
    settings_t mSettings;
    bool mLastState;
    timestamp_t mLastConnected_time;
};
