#pragma once
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>

#include "types.hpp"

enum class settings_field {
    BAT_MONITOR_MODE,
    BAT_VOLTAGE_LIMIT,
    BAT_PERCENTAGE_LIMIT,
    NET_DEVICES,
    NET_ACTIVITY_LIMIT,
    INPUT_DEVICES,
    INACT_ON_BAT_LIMIT,
    INACT_ON_CHARGER_LIMIT,
    NAME_BATTERY,
    NAME_CHARGER,
    CMD_SLEEP,
    CMD_SHUTDOWN,
    ENABLED_SLEEP,
};

class SettingsHandler {
public:
    SettingsHandler();
    ~SettingsHandler();

    settings_t getSettings();
    bool generateSettings();
    bool startDbusThread();

    void addDbusSetting(settings_field field, const std::string &content);
    void setBatteryVoltageLimit();

private:

    std::mutex mMutex;
    std::thread mDbusThread;
    settings_t mDefaultSettings;
    settings_t mSettings;
    int mAbortFD;
    std::unordered_map<settings_field, std::string> mDbusSettings;
    std::unordered_map<settings_field, std::string> mConfigFilesettings;
};
