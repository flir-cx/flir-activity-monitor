#pragma once

typedef enum class log_level {
    FATAL,
    ERROR,
    WARNING,
    NOTICE,
    INFO,
    DEBUG,
} log_level_t;


typedef enum class log_type {
    SYSLOG,
    PRINTF,
} log_type_t;


void logger_setup(log_type_t type, log_level_t level);
void logger_log(log_level_t log_level, const char *fmt, ...);
void logger_stat(const char *eventid);

