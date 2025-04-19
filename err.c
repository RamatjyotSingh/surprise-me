#include "err.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

void log_error_internal(int is_fatal, const char *file, int line, const char *func, const char *format, ...) {
    va_list args;
    // ANSI color codes
    const char *reset   = "\033[0m";
    const char *red     = "\033[31m";      // For fatal errors
    const char *yellow  = "\033[33m";      // For warnings
    const char *cyan    = "\033[36m";      // For location and timestamp info
    const char *magenta = "\033[35m";      // For system error info

    // Get current timestamp
    char timeStr[80];
    time_t rawtime = time(NULL);
    struct tm *timeinfo = localtime(&rawtime);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Print main error message
    fprintf(stderr, "%s%s:%s ", is_fatal ? red : yellow, 
                                     is_fatal ? "FATAL ERROR" : "WARNING", 
                                     reset);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");


    // Print location information
    fprintf(stderr, "%s  ↪ Location:%s %s:%d, function: %s()\n", 
            cyan, reset, file, line, func);

    // Print system error if applicable
    if (errno != 0) {
        fprintf(stderr, "%s  ↪ System Error:%s %s (errno: %d)\n", 
                magenta, reset, strerror(errno), errno);
    }

    // Append plain text (without colors) to the log file with the timestamp
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        fprintf(log_file, "[%s][%s:%d] %s() - %s: ", timeStr, file, line, func,
                is_fatal ? "FATAL ERROR" : "WARNING");
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        if (errno != 0) {
            fprintf(log_file, " (errno: %d, %s)", errno, strerror(errno));
        }
        fprintf(log_file, "\n");
        fclose(log_file);
    }
}
