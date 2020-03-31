#include "input_monitor.hpp"

#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>
#include <libudev.h>

#include "utils.hpp"
#include "log.hpp"


InputMonitor::InputMonitor(const settings_t &settings)
    : mSettings(settings)
{
}

bool
InputMonitor::start() {
    struct events_dev {
        int fd;
        struct libevdev *dev;
    };

    const int num_events = mSettings.input_event_devices.size();
    auto devices = std::vector<struct events_dev>(num_events);
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        LOG_ERROR("input_mon: epoll_create1: '%s' (%d)", strerror(errno), errno);
        return false;
    }

    struct epoll_event ev;
    mAbortFD = eventfd(0, 0);
    ev.events = EPOLLIN;
    ev.data.fd = mAbortFD;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, mAbortFD, &ev) == -1) {
        LOG_ERROR("input_mon: epoll_ctl: abort fd: '%s' (%d)", strerror(errno), errno);
        return false;
    }
    /* create udev object */
    auto udev = udev_new();
    if (!udev) {
        fprintf(stderr, "input_mon: Can't create udev\n");
        return false;
    }

    auto mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "power_supply", NULL);
    udev_monitor_enable_receiving(mon);
    int udev_fd = udev_monitor_get_fd(mon);
    ev.events = EPOLLIN;
    ev.data.fd = udev_fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, udev_fd, &ev) == -1) {
        LOG_ERROR("input_mon: epoll_ctl: udev_fd: '%s' (%d)", strerror(errno), errno);
    }


    int i = 0;
    for (const auto e: mSettings.input_event_devices) {
        LOG_DEBUG("input_mon: Adding input event: %s", e.c_str());
        int rc = 1;
        auto &dev = devices[i++];
        dev.fd = open(e.c_str(), O_RDONLY|O_NONBLOCK);
        rc = libevdev_new_from_fd(dev.fd, &dev.dev);
        if (rc < 0) {
            LOG_ERROR("input_mon: Failed to init libevdev (%s)\n", strerror(-rc));
            return false;
        }
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = dev.fd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, dev.fd, &ev) == -1) {
            LOG_ERROR("input_mon: epoll_ctl: adding event_device: '%s' (%d)", strerror(errno), errno);
            return false;
        }

    }

    reset();

    mThread = std::thread([this, epollfd, udev_fd, num_events, devices, mon] () {
    int rc = 1;
    bool stop_thread = false;
    do {
        bool charger_online = false;
        bool charger_online_changed = false;
        bool activity = false;
        struct epoll_event ep_events[num_events];
        int nfds = epoll_wait(epollfd, ep_events, num_events, -1);
        if (nfds == 0) {
            continue;
        }
        if (nfds == -1) {
            if (errno != EINTR) {
                LOG_ERROR("input_mon: epoll_wait: '%s' (%d)", strerror(errno), errno);
            }
            continue;
        }

        for (int n = 0; n < nfds; ++n) {
            if (ep_events[n].data.fd == mAbortFD) {
                stop_thread = true;
                break;
            }
            if (ep_events[n].data.fd == udev_fd) {
                const auto ps = udev_monitor_receive_device(mon);
                std::string device_name = udev_device_get_sysname(ps);
                if (device_name == mSettings.charger_name) {
                    int online = atoi(udev_device_get_sysattr_value(ps, "online"));
                    charger_online = (online == 1);
                    charger_online_changed = true;
                    // online state of power supply counts as activity as well
                    activity = true;
                    LOG_DEBUG("Power supply is %s.\n", charger_online?"ONLINE":"OFFLINE");
                }
            }
            for (const auto dev: devices) {
                if (dev.fd == ep_events[n].data.fd) {
                    LOG_DEBUG("Got input event on: %d", ep_events[n].data.fd);
                    activity = true;
                    struct input_event ev;
                    while((rc = libevdev_next_event(dev.dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == 0) {
                        // Empty evdev events for device.
                    }
                }
            }
        }

        if (stop_thread) {
            break;
        }

        if (activity) {
            const auto timestamp = get_timestamp();
            std::lock_guard<std::mutex> guard(mMutex);
            mLastInputData.event_time = timestamp;
            if (charger_online_changed) {
                mLastInputData.charger_online = charger_online;
            }
        }
    } while (true);
    }
    );

    return true;
}

InputMonitor::~InputMonitor() {
    if (mAbortFD >= 0) {
        uint64_t v = 1;
        write(mAbortFD, &v, sizeof(v));
        if (mThread.joinable()) {
            mThread.join();
        }
    }
}

input_status_t
InputMonitor::getStatus() {
    std::lock_guard<std::mutex> guard(mMutex);
    return mLastInputData;
}

void
InputMonitor::reset() {
    std::lock_guard<std::mutex> guard(mMutex);
    mLastInputData.event_time = get_timestamp();
    mLastInputData.charger_online = get_charger_online(mSettings);
}
