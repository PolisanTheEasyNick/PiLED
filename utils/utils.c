#include "../utils/utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void handle_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void logger(enum Modules module, const char *format, ...) {
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    switch (module) {
    case MAIN: {
        printf("[%sMain%s]: ", MAIN_COLOR, NO_COLOR);
        break;
    }
    case GPIO: {
        printf("[%sGPIO%s]: ", GPIO_COLOR, NO_COLOR);
        break;
    }
    case OPENRGB: {
        printf("[%sOpenRGB%s]: ", OPENRGB_COLOR, NO_COLOR);
        break;
    }
    case HTTP: {
        printf("[%sHTTP%s]: ", HTTP_COLOR, NO_COLOR);
        break;
    }
    case WS: {
        printf("[%sWebSockets%s]: ", WS_COLOR, NO_COLOR);
        break;
    }
    case ANIM: {
        printf("[%sAnimation%s]: ", ANIM_COLOR, NO_COLOR);
        break;
    }
    case TCP: {
        printf("[%sTCP%s]: ", TCP_COLOR, NO_COLOR);
        break;
    }
    case PARSER: {
        printf("[%sParser%s]: ", PARSER_COLOR, NO_COLOR);
        break;
    }
    }
    printf("[%04d-%02d-%02d %02d:%02d:%02d] ", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday, local->tm_hour,
           local->tm_min, local->tm_sec);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

void logger_debug(enum Modules module, const char *format, ...) {
#ifdef DEBUG
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    switch (module) {
    case MAIN: {
        printf("[%sMain%s]: ", MAIN_COLOR, NO_COLOR);
        break;
    }
    case GPIO: {
        printf("[%sGPIO%s]: ", GPIO_COLOR, NO_COLOR);
        break;
    }
    case OPENRGB: {
        printf("[%sOpenRGB%s]: ", OPENRGB_COLOR, NO_COLOR);
        break;
    }
    case HTTP: {
        printf("[%sHTTP%s]: ", HTTP_COLOR, NO_COLOR);
        break;
    }
    case WS: {
        printf("[%sWebSockets%s]: ", WS_COLOR, NO_COLOR);
        break;
    }
    case ANIM: {
        printf("[%sAnimation%s]: ", ANIM_COLOR, NO_COLOR);
        break;
    }
    case TCP: {
        printf("[%sTCP%s]: ", TCP_COLOR, NO_COLOR);
        break;
    }
    case PARSER: {
        printf("[%sParser%s]: ", PARSER_COLOR, NO_COLOR);
        break;
    }
    }
    printf("[%04d-%02d-%02d %02d:%02d:%02d] ", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday, local->tm_hour,
           local->tm_min, local->tm_sec);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
#endif
}