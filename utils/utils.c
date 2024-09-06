#include "../utils/utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void handle_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void logger(const char *format, ...) {
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    printf("[%04d-%02d-%02d %02d:%02d:%02d] ", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday, local->tm_hour,
           local->tm_min, local->tm_sec);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}