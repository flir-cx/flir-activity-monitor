#include "logger.hpp"
#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <cassert>
#include <stdlib.h>

namespace {
    log_level_t g_log_level = log_level_t::INFO;
    log_type_t g_log_type = log_type_t::SYSLOG;

    int log_level_to_syslog(log_level_t log_level) {
        switch(log_level) {
            case log_level_t::FATAL:
                return LOG_CRIT;

            case log_level_t::ERROR:
                return LOG_ERR;

            case log_level_t::WARNING:
                return LOG_WARNING;

            case log_level_t::NOTICE:
                return LOG_NOTICE;

            case log_level_t::INFO:
                return LOG_INFO;

            case log_level_t::DEBUG:
                return LOG_DEBUG;
        }

        // Should never be reached.
        assert(false);
        return -1;
    }
};


void logger_setup(log_type_t type, log_level_t level) {
    g_log_type = type;
    g_log_level = level;
}

void logger_log(log_level_t log_level, const char *fmt, ...) {
    if (log_level > g_log_level) {
        return;
    }

    switch (g_log_type) {
        case log_type_t::SYSLOG:
        {
            va_list ap;
            va_start(ap, fmt);
            vsyslog(log_level_to_syslog(log_level), fmt, ap);
            va_end(ap);
        }
        break;
        case log_type_t::PRINTF:
        {
            va_list ap;
            va_start(ap, fmt);
            const int len = strlen(fmt);
            char fmt_newline[len + 2];
            memcpy(fmt_newline, fmt, len);
            memcpy(fmt_newline + len, "\n", 2);
            vprintf(fmt_newline, ap);
            va_end(ap);
        }
        break;
    }
}

// Log data-collection statistics
void logger_stat(const char *eventid)
{
    char buff[256];
    snprintf(buff, sizeof(buff), "/usr/bin/collect-statistics --event-id %s --field event-source=flir-activity-monitor --field event-type=info", eventid);
    system(buff);
}
