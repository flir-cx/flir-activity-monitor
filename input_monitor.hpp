#pragma once

#include <thread>
#include <mutex>
#include <vector>

#include "types.hpp"
#include "rolling_window.hpp"


class InputMonitor {
public:
    explicit InputMonitor(const settings_t &settings);
    ~InputMonitor();
    input_status_t getStatus();
    bool start();
    void reset();

private:
    settings_t mSettings;
    std::mutex mMutex;
    std::thread mThread;
    int mAbortFD;
    input_status_t mLastInputData;
};
