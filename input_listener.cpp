#include "input_listener.hpp"
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

#include <unistd.h>
#include <sys/epoll.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>
#include <libudev.h>

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "utils.hpp"
#include "log.hpp"

namespace {

struct events_dev {
    int fd;
    struct libevdev *dev;
};

std::atomic<bool> stop_listening{false};
input_event_data_t last_input_event_data;
std::mutex m;

std::thread listener_thread;

}

int start_input_listener(const settings_t &settings) {
    const int num_events = settings.input_event_devices.size();
    auto devices = std::vector<struct events_dev>(num_events);
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        LOG_ERROR("epoll_create1: '%s' (%d)", strerror(errno), errno);
        return -1;
    }

	/* create udev object */
	auto udev = udev_new();
	if (!udev) {
		fprintf(stderr, "Can't create udev\n");
		return 1;
	}

	auto mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "power_supply", NULL);
	udev_monitor_enable_receiving(mon);
	int udev_fd = udev_monitor_get_fd(mon);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = udev_fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, udev_fd, &ev) == -1) {
        LOG_ERROR("epoll_ctl: udev_fd: '%s' (%d)", strerror(errno), errno);
    }


    int i = 0;
    for (const auto e: settings.input_event_devices) {
        LOG_DEBUG("Adding input event: %s", e.c_str());
        int rc = 1;
        auto &dev = devices[i++];
        dev.fd = open(e.c_str(), O_RDONLY|O_NONBLOCK);
        rc = libevdev_new_from_fd(dev.fd, &dev.dev);
        if (rc < 0) {
            LOG_ERROR("Failed to init libevdev (%s)\n", strerror(-rc));
            return -1;
        }
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = dev.fd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, dev.fd, &ev) == -1) {
            LOG_ERROR("epoll_ctl: adding event_device: '%s' (%d)", strerror(errno), errno);
            return -1;
        }

    }

    reset_last_input_event_data(settings);

    listener_thread = std::thread([settings, udev_fd, epollfd, num_events, devices, mon] () {
    int rc = 1;
    do {
        bool charger_online = false;
        bool charger_online_changed = false;
        bool activity = false;
        struct epoll_event ep_events[num_events];
        int nfds = epoll_wait(epollfd, ep_events, num_events, 50);
        if (nfds == 0) {
            continue;
        }
        if (nfds == -1) {
            LOG_ERROR("epoll_wait: '%s' (%d)", strerror(errno), errno);
            continue;
        }

        for (int n = 0; n < nfds; ++n) {
            if (ep_events[n].data.fd == udev_fd) {
                const auto ps = udev_monitor_receive_device(mon);
                std::string device_name = udev_device_get_sysname(ps);
                if (device_name == settings.charger_name) {
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

        if (activity) {
            const auto timestamp = get_timestamp();
            std::lock_guard<std::mutex> guard(m);
            last_input_event_data.event_time = timestamp;
            if (charger_online_changed) {
                last_input_event_data.charger_online = charger_online;
            }
        }
    } while (!stop_listening);
    }
    );

    return 0;
}

int stop_input_listener() {
    stop_listening = true;
    listener_thread.join();
    return 0;
}

input_event_data_t get_last_input_event_data() {
    std::lock_guard<std::mutex> guard(m);
    return last_input_event_data;
}

void reset_last_input_event_data(const settings_t &settings) {
    std::lock_guard<std::mutex> guard(m);
    last_input_event_data.event_time = get_timestamp();
    last_input_event_data.charger_online = get_charger_online(settings);
}
