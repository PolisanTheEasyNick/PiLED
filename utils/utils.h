#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

struct Color {
    uint8_t RED;
    uint8_t GREEN;
    uint8_t BLUE;
};

void handle_error(const char *msg);
void logger(const char *format, ...);
void logger_debug(const char *format, ...);

#endif // UTILS_H
