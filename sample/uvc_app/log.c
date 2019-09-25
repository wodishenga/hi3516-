/*
 * Log functiong for UVC app
 */

#include <stdarg.h>
#include <errno.h>
#include "log.h"

void log_info(const char *prefix, const char *file, int line,
              const char *fmt, ...)
{
    int errsv = errno;
    va_list va;

    va_start(va, fmt);
    fprintf(stderr, "%s:(%s:%d): ", prefix, file, line);
    vfprintf(stderr, fmt, va);
    va_end(va);
    errno = errsv;

    return;
}
