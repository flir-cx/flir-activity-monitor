#include "network_monitor.hpp"

#include <algorithm>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <sstream>
#include <fstream>

#include "utils.hpp"
#include "log.hpp"

namespace {

double max_net_activity;

std::vector<uint64_t> get_net_stat(const settings_t &settings, const std::string &stats_file) {
    std::vector<uint64_t> data(settings.net_devices.size());

    int idx = 0;
    for (const auto &d:settings.net_devices) {
        std::stringstream ss;
        ss << "/sys/class/net/" << d << "/statistics/" << stats_file;
        std::ifstream f(ss.str());
        if (!f.is_open()) {
            //LOG_ERROR("Failed to open statistics: '%s'", ss.str().c_str());
            data[idx++] = 0;
        } else {
            uint64_t v;
            f >> v;
            data[idx++] = v;
        }
    }
    return data;
}

uint64_t max_net_stat(std::vector<uint64_t> prev_tx, std::vector<uint64_t> prev_rx,
                      std::vector<uint64_t> curr_tx, std::vector<uint64_t> curr_rx) {
    std::vector<uint64_t> diff_tx(prev_tx.size());
    std::vector<uint64_t> diff_rx(prev_tx.size());
    std::vector<uint64_t> sum(prev_tx.size());
    std::transform(curr_tx.begin(), curr_tx.end(), prev_tx.begin(), diff_tx.begin(), std::minus<uint64_t>());
    std::transform(curr_rx.begin(), curr_rx.end(), prev_rx.begin(), diff_rx.begin(), std::minus<uint64_t>());
    std::transform(diff_tx.begin(), diff_tx.end(), diff_rx.begin(), sum.begin(), std::plus<uint64_t>());
    return *std::max_element(sum.begin(), sum.end());
}
};

bool
NetworkMonitor::start() {
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        LOG_ERROR("net_mon: epoll_create1: '%s' (%d)", strerror(errno), errno);
        return false;
    }

    struct epoll_event ev;
    mAbortFD = eventfd(0, 0);
    ev.events = EPOLLIN;
    ev.data.fd = mAbortFD;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, mAbortFD, &ev) == -1) {
        LOG_ERROR("net_mon: epoll_ctl: abort fd: '%s' (%d)", strerror(errno), errno);
        return false;
    }


    mThread = std::thread([this, epollfd] () {
    auto prev_tx_data = get_net_stat(mSettings, "tx_packets");
    auto prev_rx_data = get_net_stat(mSettings, "rx_packets");
    do {
        struct epoll_event ep_event;
        int nfds = epoll_wait(epollfd, &ep_event, 1, 10*1000);
        if (nfds == -1) {
            if (errno != EINTR) {
                LOG_ERROR("net_mon: epoll_wait: '%s' (%d)", strerror(errno), errno);
            }
            continue;
        }
        if (nfds > 0) {
            break;
        }

        auto curr_tx_data = get_net_stat(mSettings, "tx_packets");
        auto curr_rx_data = get_net_stat(mSettings, "rx_packets");
        uint64_t max_net = max_net_stat(prev_tx_data, prev_rx_data, curr_tx_data, curr_rx_data);

        std::swap(prev_tx_data, curr_tx_data);
        std::swap(prev_rx_data, curr_rx_data);

        std::lock_guard<std::mutex> guard(mMutex);
        mLastMaxTraffic = double(max_net)/10;

    } while (true);
    if (epollfd >= 0) {
        close(epollfd);
    }
    }
    );

    return true;
}

NetworkMonitor::NetworkMonitor(const settings_t &settings)
    : mSettings(settings)
{
}

NetworkMonitor::~NetworkMonitor() {
    if (mAbortFD >= 0) {
        uint64_t v = 1;
        write(mAbortFD, &v, sizeof(v));
        if (mThread.joinable()) {
            mThread.join();
        }
        close(mAbortFD);
    }
}

network_status_t
NetworkMonitor::getStatus() {
    std::lock_guard<std::mutex> guard(mMutex);
    return { .max_traffic_last_period = mLastMaxTraffic };
}

void
NetworkMonitor::reset()
{}
