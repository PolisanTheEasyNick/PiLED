#ifndef GLOBALS_H
#define GLOBALS_H

extern char *PI_ADDR;
extern char *PI_PORT;
extern char *SHARED_SECRET;
extern int RED_PIN;
extern int GREEN_PIN;
extern int BLUE_PIN;

#define PILED_VERSION 4
#define BUFFER_SIZE 55    // ver 4
#define HEADER_SIZE 18    // ver 4
#define PAYLOAD_SIZE 5    // ver 4
#define PAYLOAD_OFFSET 50 // ver 4, offset to start of PAYLOAD bytes in BUFFER

#define LED_SET_COLOR 0
#define LED_GET_CURRENT_COLOR 1
#define ANIM_SET_FADE 2
#define ANIM_SET_PULSE 3

#endif // GLOBALS_H