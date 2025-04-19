#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#define EXIT_INVAL -1
#define LOG_FILE "err.log"

#ifdef SUPPRESS_WARNINGS
#define warn_error(retval, format, ...) return retval
#define fatal_error(format, ...) exit(EXIT_FAILURE)
#else
#define fatal_error(format, ...) \
    do { \
        log_error_internal(1, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__); \
        exit(EXIT_FAILURE); \
    } while (0)

#define warn_error(retval, format, ...) \
    do { \
        log_error_internal(0, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__); \
        return retval; \
    } while (0)
#endif

void log_error_internal(int is_fatal, const char *file, int line, const char *func, const char *format, ...);

#endif
