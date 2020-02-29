#pragma once

#include "logger.hpp"

#define LOG_ERROR(...) \
    do { \
        logger_log(log_level_t::ERROR, __VA_ARGS__); \
    } while(0)

#define LOG_WARNING(...) \
    do { \
        logger_log(log_level_t::WARNING, __VA_ARGS__); \
    } while(0)

#define LOG_NOTICE(...) \
    do { \
        logger_log(log_level_t::NOTICE, __VA_ARGS__); \
    } while(0)

#define LOG_INFO(...) \
    do { \
        logger_log(log_level_t::INFO, __VA_ARGS__); \
    } while(0)

#define LOG_DEBUG(...) \
    do { \
        logger_log(log_level_t::DEBUG, __VA_ARGS__); \
    } while(0)
