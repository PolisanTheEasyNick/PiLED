#ifndef UTILS_H
#define UTILS_H

#define BUFFER_SIZE 53 // ver 2
#define SHARED_SECRET "SHARED_KEY"
#define RED_PIN 17
#define GREEN_PIN 22
#define BLUE_PIN 24

void handle_error(const char *msg);
void logger(const char *format, ...);

#endif // UTILS_H
