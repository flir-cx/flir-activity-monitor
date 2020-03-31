#pragma once

#include <algorithm>
#include <vector>
#include <mutex>
#include <sstream>

template <typename T>
class RollingWindow {
public:
    explicit RollingWindow(size_t window_size)
    : _data()
    , _winSize(window_size)
    , _nextIdx(0)
    , _nbrSamples(0)
    {
        _data.reserve(_winSize);
    };

    void reset() {
        std::lock_guard<std::mutex> lk(_m);
        _data.clear();
        _nextIdx = 0;
        _nbrSamples = 0;
    }
    void addValue(T value) {
        std::lock_guard<std::mutex> lk(_m);
        if (_nbrSamples < _winSize) {
            _data.push_back(value);
            _nbrSamples++;
        } else {
            _data[_nextIdx] = value;
        }
        _nextIdx = (_nextIdx + 1) % _winSize;
    };
    bool isFullyPopulated() {
         std::lock_guard<std::mutex> lk(_m);
         return _nbrSamples == _winSize;
    };
    bool allValuesAreBelow(T limit) {
         std::lock_guard<std::mutex> lk(_m);
         return std::all_of(_data.cbegin(), _data.cend(), [limit](T v){ return v < limit; });
    };
    std::string getDataAsString() {
        std::stringstream ss;
        int idx = 0;
        int curr_idx = (_nextIdx == 0)? _nbrSamples - 1: _nextIdx - 1;
        for (const auto v:_data) {
            if (idx++ == curr_idx) {
                ss << "[ " << v << " ] ";
            } else {
                ss << v << " ";
            }
        }
        return ss.str();
    }
private:
    std::vector<T> _data;
    size_t _winSize;
    size_t _nextIdx;
    size_t _nbrSamples;
    std::mutex _m;
};
