#pragma once

#include <thread>
#include <mutex>
#include <vector>

#include "types.hpp"


class NetworkMonitor {
public:
    explicit NetworkMonitor(const settings_t &settings);
    ~NetworkMonitor();
    network_status_t getStatus();
    bool start();
    void reset();

private:
    settings_t mSettings;
    std::mutex mMutex;
    std::thread mThread;
    double mLastMaxTraffic;
    int mAbortFD;
};
