#pragma once

#include <syslog.h>

#define LOG_ERROR(...) \
    do { \
        syslog(LOG_ERR, __VA_ARGS__); \
    } while(0)

#define LOG_WARNING_LEVEL 4
#undef LOG_WARNING
#define LOG_WARNING(...) \
    do { \
        syslog(LOG_WARNING_LEVEL, __VA_ARGS__); \
    } while(0)

#define LOG_NOTICE_LEVEL 5
#undef LOG_NOTICE
#define LOG_NOTICE(...) \
    do { \
        syslog(LOG_NOTICE_LEVEL, __VA_ARGS__); \
    } while(0)

#define LOG_INFO_LEVEL 6
#undef LOG_INFO
#define LOG_INFO(...) \
    do { \
        syslog(LOG_INFO_LEVEL, __VA_ARGS__); \
    } while(0)

#define LOG_DEBUG_LEVEL 7
#undef LOG_DEBUG
#define LOG_DEBUG(...) \
    do { \
        syslog(LOG_DEBUG_LEVEL, __VA_ARGS__); \
    } while(0)
