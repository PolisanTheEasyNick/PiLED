#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>

struct openrgb_device {
    uint32_t device_id;
    uint8_t *name;
};

extern char *PI_ADDR;
extern char *PI_PORT;
extern char *SHARED_SECRET;
extern int RED_PIN;
extern int GREEN_PIN;
extern int BLUE_PIN;
extern char *OPENRGB_SERVER;
extern int OPENRGB_PORT;

extern struct openrgb_device *openrgb_devices_to_change; // defined in openrgb.c
extern int32_t openrgb_using_devices_num;                // defined in openrgb.c

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