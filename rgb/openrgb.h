#ifndef OPENRGB_H
#define OPENRGB_H

#include "../utils/utils.h"
#include <pthread.h>
#include <signal.h>
#include <stdint.h>

// Packet ID
#define OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT 0
#define OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_DATA 1
#define OPENRGB_NET_PACKET_ID_REQUEST_PROTOCOL_VERSION 40
#define OPENRGB_NET_PACKET_ID_SET_CLIENT_NAME 50
#define OPENRGB_NET_PACKET_ID_DEVICE_LIST_UPDATED 100
#define OPENRGB_NET_PACKET_ID_REQUEST_PROFILE_LIST 150
#define OPENRGB_NET_PACKET_ID_REQUEST_SAVE_PROFILE 151
#define OPENRGB_NET_PACKET_ID_REQUEST_LOAD_PROFILE 152
#define OPENRGB_NET_PACKET_ID_REQUEST_DELETE_PROFILE 153
#define OPENRGB_NET_PACKET_ID_RGBCONTROLLER_RESIZEZONE 1000
#define OPENRGB_NET_PACKET_ID_RGBCONTROLLER_UPDATELEDS 1050
#define OPENRGB_NET_PACKET_ID_RGBCONTROLLER_UPDATEZONELEDS 1051
#define OPENRGB_NET_PACKET_ID_RGBCONTROLLER_UPDATESINGLELED 1052
#define OPENRGB_NET_PACKET_ID_RGBCONTROLLER_SETCUSTOMMODE 1100
#define OPENRGB_NET_PACKET_ID_RGBCONTROLLER_UPDATEMODE 1101
#define OPENRGB_NET_PACKET_ID_RGBCONTROLLER_SAVEMODE 1102

#define OPENRGB_SUPPORTED_VERSION 4

extern int openrgb_socket;
extern pthread_t openrgb_recv_thread_id;
extern pthread_mutex_t openrgb_send_mutex;
extern int8_t openrgb_using_version;
extern int32_t openrgb_devices_num;
extern struct openrgb_controller_data *openrgb_controllers;
extern int8_t openrgb_parsed_all_devices;
extern volatile sig_atomic_t openrgb_stop_server;

struct openrgb_controller_data {
    // NET_PACKET_ID_REQUEST_CONTROLLER_DATA response type for version 3 of OpenRGB SDK
    uint32_t data_size; // size of all data in packet
    int32_t type;       // RGBController type field value

    uint16_t name_len; // Length of RGBController name field string, including null termination
    uint8_t *name;     // RGBController name field string value, including null termination

    uint16_t vendor_len; // Length of RGBController vendor field string, including null termination
    uint8_t *vendor;     // RGBController vendor field string value, including null termination

    uint16_t description_len; // Length of RGBController description field string, including null termination
    uint8_t *description;     // RGBController description field string value, including null termination

    uint16_t version_len; // Length of RGBController version field string, including null termination
    uint8_t *version;     // RGBController version field string value, including null termination

    uint16_t serial_len; // Length of RGBController serial field string, including null termination
    uint8_t *serial;     // RGBController serial field string value, including null termination

    uint16_t location_len; // Length of RGBController location field string, including null termination
    uint8_t *location;     // RGBController location field string value, including null termination

    uint16_t num_modes;              // Number of modes in RGBController
    uint8_t active_mode;             // RGBController active_mode field value
    struct openrgb_mode_data *modes; // See Mode Data block format table.  Repeat num_modes times

    uint16_t num_zones;              // Number of zones in RGBController
    struct openrgb_zone_data *zones; // See Zone Data block format table.  Repeat num_zones times

    uint16_t num_leds;             // Number of LEDs in RGBController
    struct openrgb_led_data *leds; // See LED Data block format table.  Repeat num_leds times

    uint16_t num_colors; // Number of colors in RGBController
    uint8_t *colors;     // RGBController colors field values
};

struct openrgb_mode_data {
    uint16_t mode_name_len;       // Length of mode name string, including null termination
    uint8_t *mode_name;           // Mode name string value, including null termination
    uint32_t mode_value;          // Mode value field value
    uint32_t mode_flags;          // Mode flags field value
    uint32_t mode_speed_min;      // Mode speed_min field value
    uint32_t mode_speed_max;      // Mode speed_max field value
    uint32_t mode_brightness_min; // Mode brightness_min field value
    uint32_t mode_brightness_max; // Mode brightness_max field value
    uint32_t mode_colors_min;     // Mode colors_min field value
    uint32_t mode_colors_max;     // Mode colors_max field value
    uint32_t mode_speed;          // Mode speed value
    uint32_t mode_brightness;     // Mode brightness value
    uint32_t mode_direction;      // Mode direction value
    uint32_t mode_color_mode;     // Mode color_mode value
    uint32_t mode_num_colors;     // Mode number of colors
    uint32_t *mode_colors;        // Mode color values
};

struct openrgb_zone_data {
    uint16_t zone_name_len;
    uint8_t *zone_name;
    uint32_t zone_type;
    uint32_t zone_leds_min;
    uint32_t zone_leds_max;
    uint32_t zone_leds_count;
    uint16_t zone_matrix_len;
    uint32_t zone_matrix_height;
    uint32_t zone_matrix_width;
    uint32_t *zone_matrix_data;
};

struct openrgb_led_data {
    uint16_t led_name_len;
    uint8_t *led_name;
    uint32_t led_value;
};

void openrgb_init_header(uint8_t **header, uint32_t pkt_dev_idx, uint32_t pkt_id, uint32_t pkg_size);
void openrgb_init();
void openrgb_shutdown();
void openrgb_request_protocol_version();
void openrgb_set_client_name();
void openrgb_request_controller_count();
void openrgb_request_controller_data(uint32_t pkt_dev_idx);
void openrgb_request_update_leds(uint32_t pkt_dev_idx, struct Color color);

void *openrgb_recv_thread(void *arg);

#endif
