#ifndef COLORS_H
#define COLORS_H

// ANSI color codes
#define ANSI_RESET          "\033[0m"
#define ANSI_BLACK          "\033[30m"
#define ANSI_RED            "\033[31m"
#define ANSI_GREEN          "\033[32m"
#define ANSI_YELLOW         "\033[33m"
#define ANSI_BLUE           "\033[34m"
#define ANSI_MAGENTA        "\033[35m"
#define ANSI_CYAN           "\033[36m"
#define ANSI_WHITE          "\033[37m"

// Bright variants
#define ANSI_BRIGHT_BLACK   "\033[90m"
#define ANSI_BRIGHT_RED     "\033[91m"
#define ANSI_BRIGHT_GREEN   "\033[92m"
#define ANSI_BRIGHT_YELLOW  "\033[93m"
#define ANSI_BRIGHT_BLUE    "\033[94m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN    "\033[96m"
#define ANSI_BRIGHT_WHITE   "\033[97m"

// Background colors
#define ANSI_BG_BLACK       "\033[40m"
#define ANSI_BG_RED         "\033[41m"
#define ANSI_BG_GREEN       "\033[42m"
#define ANSI_BG_YELLOW      "\033[43m"
#define ANSI_BG_BLUE        "\033[44m"
#define ANSI_BG_MAGENTA     "\033[45m"
#define ANSI_BG_CYAN        "\033[46m"
#define ANSI_BG_WHITE       "\033[47m"

// Text styles
#define ANSI_BOLD           "\033[1m"
#define ANSI_UNDERLINE      "\033[4m"
#define ANSI_ITALIC         "\033[3m"

// Clear screen
#define ANSI_CLEAR_SCREEN   "\033[2J\033[H"

#endif // COLORS_H