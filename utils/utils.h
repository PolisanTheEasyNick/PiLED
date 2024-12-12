#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

struct Color {
    uint8_t RED;
    uint8_t GREEN;
    uint8_t BLUE;
};

enum Modules { MAIN = 1, GPIO, OPENRGB, HTTP, WS, ANIM, TCP, PARSER };

void handle_error(const char *msg);
void logger(enum Modules module, const char *format, ...);
void logger_debug(enum Modules module, const char *format, ...);

// Module Name -> color logging.
#define MAIN_COLOR "\033[38;5;206m"  // Pink
#define GPIO_COLOR "\033[38;5;21m"   // Blue
#define OPENRGB_COLOR "\033[38;5;9m" // Red
#define HTTP_COLOR "\033[38;5;141m"  // light purple
#define WS_COLOR "\033[38;5;202m"    // Orange
#define ANIM_COLOR "\033[38;5;82m"   // Light green
#define TCP_COLOR "\033[38;5;6m"     // Cyan
#define PARSER_COLOR "\033[38;5;52m" // Dark Red
#define NO_COLOR "\033[0m"

#endif // UTILS_H
