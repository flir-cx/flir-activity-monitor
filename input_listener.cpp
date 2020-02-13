#include "input_listener.hpp"
#include <memory>
#include <atomic>
#include <thread>

#include <unistd.h>
#include <sys/epoll.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "utils.hpp"
#include "log.h"

namespace {

struct events_dev {
    int fd;
    struct libevdev *dev;
};

std::atomic<bool> stop_listening{false};
std::atomic<timestamp_t> last_input_event_time;

std::thread listener_thread;

}

int start_input_listener(const settings_t &settings) {
    listener_thread = std::thread([settings] () {
    const int num_events = settings.input_event_devices.size();
    last_input_event_time = get_timestamp();

    auto devices = std::vector<struct events_dev>(num_events);
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        LOG_ERROR("epoll_create1: '%s' (%d)", strerror(errno), errno);
        return;
    }


    int i = 0;
    for (const auto e: settings.input_event_devices) {
        LOG_INFO("Adding input event: %s", e.c_str());
        int rc = 1;
        auto &dev = devices[i++];
        dev.fd = open(e.c_str(), O_RDONLY|O_NONBLOCK);
        rc = libevdev_new_from_fd(dev.fd, &dev.dev);
        if (rc < 0) {
            LOG_ERROR("Failed to init libevdev (%s)\n", strerror(-rc));
            return;
        }
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = dev.fd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, dev.fd, &ev) == -1) {
            LOG_ERROR("epoll_ctl: listen_sock: '%s' (%d)", strerror(errno), errno);
            return;
        }

    }

    struct libevdev *dev = NULL;
    int rc = 1;
    do {
        struct epoll_event ep_events[num_events];
        int nfds = epoll_wait(epollfd, ep_events, num_events, 50);
        if (nfds == 0) {
            continue;
        }
        if (nfds == -1) {
            LOG_ERROR("epoll_wait: '%s' (%d)", strerror(errno), errno);
            continue;
        }
        const auto timestamp = get_timestamp();
        last_input_event_time = timestamp;

        for (int n = 0; n < nfds; ++n) {
            LOG_DEBUG("Got event on: %d", ep_events[n].data.fd);
            for (const auto dev: devices) {
                if (dev.fd == ep_events[n].data.fd) {
                    struct input_event ev;
                    while((rc = libevdev_next_event(dev.dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == 0) {
 //                       printf("Event: %s %s %d\n",
 //                               libevdev_event_type_get_name(ev.type),
 //                               libevdev_event_code_get_name(ev.type, ev.code),
 //                               ev.value);
                    }
                }
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

timestamp_t get_last_input_event_time() {
    return last_input_event_time.load();
}

void reset_last_input_event_time() {
    last_input_event_time = get_timestamp();
}
