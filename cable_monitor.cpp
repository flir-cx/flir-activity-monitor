#include "cable_monitor.hpp"
#include "utils.hpp"

#include <fstream>
#include <unistd.h>
#include <string.h>
#include <string>

#include "log.hpp"

namespace {

bool get_cable_connected() {
  /*
    Known possible values read from /etc/sysfs-links/usb2_control/state :

    "configured"   - when there is a function enabled - "cable used/connected"
    "suspended"    - when connected to for instance a powered hub not connected to a computer
    "powered"      - connected to a charger
    "not attached" - no cable attached (get_value_from_file() below will return only "not" )
   */
    bool rval;
    // sysfs-links are setup on os to point to correct location
    std::string cable_file = "/etc/sysfs-links/";
    cable_file += "usb2_control/state";
    std::string unknown_state = "unknown";
    std::string curstate = get_value_from_file(cable_file, unknown_state);

    rval = (curstate == "configured");    
    
    return rval;
}


}


CableMonitor::CableMonitor(const settings_t settings)
: mSettings(settings)
{
    mLastConnected_time = get_timestamp();    
}

CableMonitor::~CableMonitor() {
}

cable_status_t
CableMonitor::getStatus() {
    bool curstate = get_cable_connected();
    if (curstate)
        mLastConnected_time = get_timestamp();
    if (curstate != mLastState)
    {
        LOG_INFO("CableMonitor - new connectedState:%d", curstate);
        mLastState = curstate;
    }
    return {
        .online = curstate,
        .lastconnected_time = mLastConnected_time
    };
}

void
CableMonitor::printData() {
    LOG_INFO("Cable: '%s'", "xxx");
}
