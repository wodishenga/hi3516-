#ifndef __UVC_LOG_H__
#define __UVC_LOG_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define RLOG(format, ...) fprintf(stderr, "\033[;31m" format "\033[0m\n", ## __VA_ARGS__)

#define INFO(...) log_info("INFO", __func__, __LINE__, __VA_ARGS__)
#define ERR(...) log_info("Error", __func__, __LINE__, __VA_ARGS__)
#define ERR_ON(cond, ...) ((cond) ? ERR(__VA_ARGS__) : 0)

#define CRIT(...) \
    do { \
        log_info("Critical", __func__, __LINE__, __VA_ARGS__); \
        exit(0); \
    } while (0)

#define CRIT_ON(cond, ...) do { if (cond) CRIT(__VA_ARGS__);} while (0)

void log_info(const char *prefix, const char *file, int line, const char *fmt, ...);

#endif //__UVC_LOG_H__
