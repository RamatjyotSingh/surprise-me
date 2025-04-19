#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "colors.h"   // ← pull in ANSI color macros

#define EXIT_INVAL -1
#define LOG_FILE   "err.log"

#ifdef SUPPRESS_WARNINGS
  #define warn_error(retval, format, ...) return retval
  #define fatal_error(format, ...)           exit(EXIT_FAILURE)
#else
  #define fatal_error(format, ...)                                   \
    do {                                                             \
      log_error_internal(1, __FILE__, __LINE__, __func__,            \
                         format, ##__VA_ARGS__);                     \
      exit(EXIT_FAILURE);                                            \
    } while (0)

  #define warn_error(retval, format, ...)                            \
    do {                                                             \
      log_error_internal(0, __FILE__, __LINE__, __func__,            \
                         format, ##__VA_ARGS__);                     \
      return retval;                                                 \
    } while (0)
#endif

/*
 * Enhanced user interface message types with different colors
 * for different types of messages
 */
 
// Error messages - red
#define user_error(format, ...)                                      \
  do {                                                               \
    fprintf(stderr, ANSI_RED "Error: " format ANSI_RESET "\n",       \
            ##__VA_ARGS__);                                          \
  } while (0)

// Fatal errors - bright red (terminates program)
#define user_fatal(format, ...)                                      \
  do {                                                               \
    fprintf(stderr, ANSI_RED "Fatal: " format ANSI_RESET "\n",      \
            ##__VA_ARGS__);                                          \
    exit(EXIT_FAILURE);                                              \
  } while (0)

// Warning messages - yellow
#define user_warning(format, ...)                                    \
  do {                                                               \
    fprintf(stderr, ANSI_YELLOW "Warning: " format ANSI_RESET "\n",  \
            ##__VA_ARGS__);                                          \
  } while (0)

// Information messages - blue
#define user_info(format, ...)                                       \
  do {                                                               \
    fprintf(stdout, ANSI_BLUE "Info: " format ANSI_RESET "\n",       \
            ##__VA_ARGS__);                                          \
  } while (0)

// Success messages - green
#define user_success(format, ...)                                    \
  do {                                                               \
    fprintf(stdout, ANSI_GREEN "Success: " format ANSI_RESET "\n",   \
            ##__VA_ARGS__);                                          \
  } while (0)

// Response messages - cyan (for general user feedback)
#define user_response(format, ...)                                   \
  do {                                                               \
    fprintf(stdout, ANSI_CYAN format ANSI_RESET "\n",                \
            ##__VA_ARGS__);                                          \
  } while (0)

// Prompt messages - magenta (for interactive prompts)
#define user_prompt(format, ...)                                     \
  do {                                                               \
    fprintf(stdout, ANSI_MAGENTA "? " format ANSI_RESET " ",         \
            ##__VA_ARGS__);                                          \
    fflush(stdout);                                                  \
  } while (0)

/* existing public API */
void log_error_internal(int is_fatal,
                        const char *file, int line,
                        const char *func,
                        const char *format, ...);

#endif // ERROR_H
