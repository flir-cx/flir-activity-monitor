#pragma once

#include <thread>
#include <mutex>
#include <vector>

#include "types.hpp"
#include "rolling_window.hpp"


class BatteryMonitor {
public:
    BatteryMonitor(const settings_t settings,
                   size_t nbr_samples,
                   int sample_period_ms);
    ~BatteryMonitor();
    battery_status_t getStatus();
    bool start();
    void reset();
    void printData();

private:
    settings_t mSettings;
    size_t mNumberSamples;
    int mSamplePeriod;
    std::mutex mMutex;
    std::thread mThread;
    int mAbortFD;
    RollingWindow<double> mBatteryVoltage;
    RollingWindow<double> mBatteryCapacity;
};
