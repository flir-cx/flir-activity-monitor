#include "battery_monitor.hpp"

#include <fstream>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <string.h>

#include "log.hpp"

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

double get_battery_voltage(const settings_t &settings) {
    std::string voltage_file = "/sys/class/power_supply/";
    voltage_file += settings.battery_name;
    voltage_file += "/voltage_now";
    int voltage = get_value_from_file(voltage_file, -1000000);
    return double(voltage)/1000000;
}

double get_battery_capacity(const settings_t &settings) {
    std::string capacity_file = "/sys/class/power_supply/";
    capacity_file += settings.battery_name;
    capacity_file += "/capacity";
    int capacity = get_value_from_file(capacity_file, -1);
    return capacity;
}
}

BatteryMonitor::BatteryMonitor(const settings_t settings,
                   size_t nbr_samples,
                   int sample_period_ms)
: mSettings(settings)
, mNumberSamples(nbr_samples)
, mSamplePeriod(sample_period_ms)
, mBatteryVoltage(mNumberSamples)
, mBatteryCapacity(mNumberSamples)
{
}

BatteryMonitor::~BatteryMonitor() {
    if (mAbortFD >= 0) {
        uint64_t v = 1;
        write(mAbortFD, &v, sizeof(v));
        if (mThread.joinable()) {
            mThread.join();
        }
        close(mAbortFD);
    }
}

void
BatteryMonitor::reset() {
    mBatteryVoltage.reset();
    mBatteryCapacity.reset();
}


bool
BatteryMonitor::start() {
    int epollfd = epoll_create1(0);
    struct epoll_event ev;
    mAbortFD = eventfd(0, 0);
    ev.events = EPOLLIN;
    ev.data.fd = mAbortFD;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, mAbortFD, &ev) == -1) {
        LOG_ERROR("bat_mon: epoll_ctl: sd_bus fd: '%s' (%d)", strerror(errno), errno);
        return false;
    }

    mBatteryVoltage.addValue(get_battery_voltage(mSettings));
    mBatteryCapacity.addValue(get_battery_capacity(mSettings));

    mThread = std::thread([this, epollfd] () mutable
    {
        bool stop_thread = false;
        for (;;) {
            struct epoll_event ep_events[1];
            int nfds = epoll_wait(epollfd, ep_events, 1, mSamplePeriod);
            if (nfds == -1) {
                if (errno != EINTR) {
                    LOG_ERROR("bat_mon: epoll_wait: '%s' (%d)", strerror(errno), errno);
                }
                continue;
            }
            for (int n = 0; n < nfds; ++n) {
                if (ep_events[n].data.fd == mAbortFD) {
                    stop_thread = true;
                    break;
                }
            }
            if (stop_thread) {
                break;
            }
            auto voltage = get_battery_voltage(mSettings);
            mBatteryVoltage.addValue(voltage);
            auto capacity = get_battery_capacity(mSettings);
            mBatteryCapacity.addValue(capacity);
        }
        if (epollfd >= 0) {
            close(epollfd);
        }
    });

    return true;
}

battery_status_t
BatteryMonitor::getStatus() {
    return {
        .valid = mBatteryVoltage.isFullyPopulated(),
        .voltage_below_limit = mBatteryVoltage.allValuesAreBelow(mSettings.battery_voltage_limit),
        .capacity_below_limit = mBatteryCapacity.allValuesAreBelow(mSettings.battery_capacity_limit),
    };
}

void
BatteryMonitor::printData() {
    const std::string voltages = mBatteryVoltage.getDataAsString();
    const std::string capacities = mBatteryCapacity.getDataAsString();
    LOG_INFO("Voltages: '%s'\nCapacity: '%s'", voltages.c_str(), capacities.c_str());
}
