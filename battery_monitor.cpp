#include "battery_monitor.hpp"
#include "utils.hpp"

#include <fstream>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <string.h>
#include <string>

#include "log.hpp"

namespace {

double get_battery_voltage(const settings_t &settings) {
    // sysfs-links are setup on os to point to correct location
    std::string voltage_file = "/etc/sysfs-links/";
    voltage_file += settings.battery_name;
    voltage_file += "/voltage_now";
    int voltage = get_value_from_file(voltage_file, -1000000);
    return double(voltage)/1000000;
}

double get_battery_capacity(const settings_t &settings) {
    // sysfs-links are setup on os to point to correct location
    std::string capacity_file = "/etc/sysfs-links/";
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
        bool err_voltage = false;
        bool err_capacity = false;
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
            if (voltage >= 0) {
                mBatteryVoltage.addValue(voltage);
                if (err_voltage) {
                    LOG_INFO("get_battery_voltage restored");
                    err_voltage = false;
                }
            } else {
                if (!err_voltage)
                {
                    err_voltage = true;
                    LOG_WARNING("get_battery_voltage failed");
                }
            }

            auto capacity = get_battery_capacity(mSettings);
            if (capacity >= 0) {
                mBatteryCapacity.addValue(capacity);
                if (err_capacity) {
                    LOG_INFO("get_battery_capacity restored");
                    err_capacity = false;
                }
            } else {
                if (!err_capacity) {
                    LOG_WARNING("get_battery_capacity failed, battery out?");
                    err_capacity = true;
                }
            }
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
