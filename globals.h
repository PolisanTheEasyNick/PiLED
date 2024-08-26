#ifndef GLOBALS_H
#define GLOBALS_H

extern char *PI_ADDR;
extern char *PI_PORT;
extern char *SHARED_SECRET;
extern int RED_PIN;
extern int GREEN_PIN;
extern int BLUE_PIN;

#define BUFFER_SIZE 54    // ver 3
#define HEADER_SIZE 18    // ver 3
#define PAYLOAD_SIZE 4    // ver 3
#define PAYLOAD_OFFSET 50 // ver 3, offset to start of PAYLOAD bytes in BUFFER

#define LED_SET_COLOR 0
#define LED_GET_CURRENT_COLOR 1

#endif // GLOBALS_H