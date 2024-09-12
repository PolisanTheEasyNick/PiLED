#include "openrgb.h"
#include "../globals/globals.h"
#include "../parser/parser.h"
#include "../utils/utils.h"
#include <arpa/inet.h>
#include <bits/pthreadtypes.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

int openrgb_socket;
pthread_t openrgb_recv_thread_id;
pthread_mutex_t openrgb_send_mutex;
int8_t openrgb_using_version = -1;
int32_t openrgb_devices_num = -1;
int32_t openrgb_using_devices_num = 0;
int8_t openrgb_parsed_all_devices = -1;
struct openrgb_controller_data *openrgb_controllers;
volatile sig_atomic_t openrgb_stop_server = 0;
struct openrgb_device *openrgb_devices_to_change;

void openrgb_init_header(uint8_t **header, uint32_t pkt_dev_idx, uint32_t pkt_id, uint32_t pkg_size) {
    // as per 11.09.2024 only God and me knows how pointers works here
    // as per future only God will know.
    *header = malloc(16);
    // adding magick
    (*header)[0] = 'O';
    (*header)[1] = 'R';
    (*header)[2] = 'G';
    (*header)[3] = 'B';
    memcpy(((*header) + 4), &pkt_dev_idx, 4);
    memcpy(((*header) + 8), &pkt_id, 4);
    memcpy(((*header) + 12), &pkg_size, 4);
#ifdef DEBUG
    printf("OpenRGB: Generated header\n");
    for (int i = 0; i < 16; i++) {
        printf("%x ", (*header)[i]);
    }
    printf("\n");
#endif
}

void openrgb_init() {
    logger_debug("Initializing OpenRGB!");
    // connects to OpenRGB server, negotiates OpenRGB's SDK version and listens for responses
    if (pthread_mutex_init(&openrgb_send_mutex, NULL) != 0) {
        logger_debug("Mutex init failed\n");
        return;
    }

    struct sockaddr_in server_addr;
    openrgb_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (openrgb_socket < 0) {
        logger_debug("Failed to create socket");
        return;
    }
    logger_debug("Created socket.");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6742);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        logger_debug("Invalid OpenRGB server's IP address or IP address not supported");
        close(openrgb_socket);
        return;
    }

    if (connect(openrgb_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        logger_debug("Connection failed");
        return;
    }
    logger_debug("Connected to OpenRGB Server.");

    // at this state we successfully connected to OpenRGB server, starting recv thread

    if (pthread_create(&openrgb_recv_thread_id, NULL, openrgb_recv_thread, NULL) != 0) {
        logger_debug("Failed to create receive thread");
        close(openrgb_socket);
        return;
    }

    // now all responses by openrgb will be received by recv thread.
    // requesting version.
    openrgb_request_protocol_version();
    logger_debug("Requested protocol version, waiting...\n");
    uint8_t timeout_counter = 0;
    while (openrgb_using_version == -1) {
        logger_debug("Waiting... current openrgb version: %d\n", openrgb_using_version);
        usleep(100000);
        timeout_counter++;
        if (timeout_counter > 10) {
            // if no protocol version received within 1s, assuming that server does not
            // supports protocol versioning and working at version 0
            openrgb_using_version = 0;
            break;
        }
    }
    logger_debug("OpenRGB negotiated version is %d!", openrgb_using_version);

    logger_debug("Setting client's name\n");
    openrgb_set_client_name();

    logger_debug("Getting current devices number");
    timeout_counter = 0;
    openrgb_request_controller_count();
    while (openrgb_devices_num == -1) {
        logger_debug("Waiting... current openrgb version: %d", openrgb_using_version);
        usleep(100000);
        timeout_counter++;
        if (timeout_counter > 20) {
            logger_debug("Error! Can't get devices number!");
            openrgb_devices_num = 0;
            return;
            break;
        }
    }
    logger_debug("Got %d devices count", openrgb_devices_num);

    openrgb_controllers = malloc(openrgb_devices_num * sizeof(struct openrgb_controller_data));

    for (int device = 0; device < openrgb_devices_num; device++) {
        logger_debug("Getting device %d data...", device);
        openrgb_request_controller_data(device);
    }

    logger_debug("Requested all controller data.");
    while (openrgb_parsed_all_devices == -1) {
        logger_debug("Waiting for all devices...");
        usleep(200000);
        timeout_counter++;
        if (timeout_counter > 20) {
            logger_debug("Error! Can't parse all device or timeout!");
            break;
        }
    }

#ifndef ORGBCONFIGURATOR
    logger_debug("Parsing device preferences config...");
    parse_openrgb_config_devices("/etc/piled/openrgb_config");
#endif
}

void openrgb_request_protocol_version() {
    logger_debug("Requesting protocol version.");
    uint8_t *header = NULL;
    openrgb_init_header(&header, 0, OPENRGB_NET_PACKET_ID_REQUEST_PROTOCOL_VERSION, 4);
    uint32_t version = OPENRGB_SUPPORTED_VERSION;
    pthread_mutex_lock(&openrgb_send_mutex);
#ifdef DEBUG
    printf("Sending header:\n");
    for (int i = 0; i < 16; i++) {
        printf("%x ", header[i]);
    }
    printf("\n");
#endif
    send(openrgb_socket, header, 16, 0);
#ifdef DEBUG
    printf("Sending version: %d\n", version);
#endif
    send(openrgb_socket, &version, 4, 0);
    pthread_mutex_unlock(&openrgb_send_mutex);
    free(header);
}

void openrgb_set_client_name() {
    uint8_t *name = malloc(strlen("piled vx"));
    strcpy((char *)name, "PiLED v");
    name[7] = PILED_VERSION + '0';
    uint8_t *header = NULL;
    openrgb_init_header(&header, 0, OPENRGB_NET_PACKET_ID_SET_CLIENT_NAME, strlen("piled vx"));
    pthread_mutex_lock(&openrgb_send_mutex);
    send(openrgb_socket, header, 16, 0);
    send(openrgb_socket, name, strlen("piled vx"), 0);
    pthread_mutex_unlock(&openrgb_send_mutex);
    free(header);
    free(name);
}

void openrgb_request_controller_count() {
    logger_debug("Requesting controller count");
    uint8_t *header;
    openrgb_init_header(&header, 0, OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT, 0);
    pthread_mutex_lock(&openrgb_send_mutex);
    send(openrgb_socket, header, 16, 0);
    pthread_mutex_unlock(&openrgb_send_mutex);
    free(header);
}

void openrgb_request_controller_data(uint32_t pkt_dev_idx) {
    logger_debug("Requesting controller data");
    uint8_t *header;
    openrgb_init_header(&header, pkt_dev_idx, OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_DATA, 4);
    pthread_mutex_lock(&openrgb_send_mutex);
    send(openrgb_socket, header, 16, 0);
    send(openrgb_socket, &openrgb_using_version, 4, 0);
    pthread_mutex_unlock(&openrgb_send_mutex);
    free(header);
}

void openrgb_request_update_leds(uint32_t pkt_dev_idx, struct Color color) {
    logger_debug("Setting controller #%d color", pkt_dev_idx);

    uint32_t packet_size = 4 +                                            // data_size
                           2 +                                            // num_colors
                           4 * openrgb_controllers[pkt_dev_idx].num_leds; // led_color

    uint8_t *header;
    openrgb_init_header(&header, pkt_dev_idx, OPENRGB_NET_PACKET_ID_RGBCONTROLLER_UPDATELEDS, packet_size);

    uint8_t packet[packet_size];
    memcpy(packet, &packet_size, 4);
    memcpy(packet + 4, &openrgb_controllers[pkt_dev_idx].num_leds, 2);
#ifdef DEBUG
    printf("Num leds for #%d device is %d\n", pkt_dev_idx, openrgb_controllers[pkt_dev_idx].num_leds);
#endif
    for (int color_idx = 0; color_idx < openrgb_controllers[pkt_dev_idx].num_leds; color_idx++) {
        uint32_t color_data = 0;
        color_data |= (color.BLUE & 0xFF) << 16;
        color_data |= (color.GREEN & 0xFF) << 8;
        color_data |= (color.RED & 0xFF);
        logger_debug("red: %x, green: %x, blue: %x\nColor data: %02x\n", color.RED, color.GREEN, color.BLUE,
                     color_data);
        memcpy(packet + 6 + 4 * color_idx, &color_data, 4);
    }
#ifdef DEBUG
    printf("Sending color packet:\n");
    for (int i = 0; i < packet_size; i++) {
        printf("%02x ", packet[i]);
    }
    printf("\n");
#endif
    pthread_mutex_lock(&openrgb_send_mutex);
    send(openrgb_socket, header, 16, 0);
    send(openrgb_socket, packet, packet_size, 0);
    pthread_mutex_unlock(&openrgb_send_mutex);
}

void openrgb_set_color_on_devices(struct Color color) {
    for (uint16_t device = 0; device < openrgb_using_devices_num; device++) {
        openrgb_request_update_leds(openrgb_devices_to_change[device].device_id, color);
    }
}

void *openrgb_recv_thread(void *arg) {
    logger_debug("Started OpenRGB receive thread!\n");
    uint8_t header_recv[16];
    int recv_size;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    if (setsockopt(openrgb_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        logger_debug("Failed to set socket timeout");
    }

    while (!openrgb_stop_server) {
        recv_size = recv(openrgb_socket, header_recv, sizeof(header_recv), 0);
        if (recv_size < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;
            }
            logger_debug("Failed to receive data");
            break;
        }

        if (recv_size == 0) {
            logger_debug("Connection closed by peer\n");
            break;
        }

        logger_debug("OpenRGB Parser: Starting parsing buffer with size %d", recv_size);
#ifdef DEBUG
        printf("Buffer:\n");
        for (int i = 0; i < recv_size; i++) {
            printf("%x ", header_recv[i]);
        }
        printf("\n");
#endif

        // сhecking for magick
        if (header_recv[0] != 'O' || header_recv[1] != 'R' || header_recv[2] != 'G' || header_recv[3] != 'B') {
            logger_debug("Magick wrong! Not a OpenRGB package!\n");
            return NULL;
        }

        uint32_t pkt_dev_idx = 0;
        memcpy(&pkt_dev_idx, header_recv + 4, 4);
        logger_debug("Got device index: %d\n", pkt_dev_idx);

        uint32_t pkt_id = 0;
        memcpy(&pkt_id, header_recv + 8, 4);
        logger_debug("Got packet id: %d\n", pkt_id);

        uint32_t pkt_size = 0;
        memcpy(&pkt_size, header_recv + 12, 4);
        logger_debug("Got packet size: %d\n", pkt_size);
        // entire header received, now receive the data
        uint8_t response[pkt_size];
        switch (pkt_id) {
        case OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_DATA: {
            logger_debug("NET_PACKET_ID_REQUEST_CONTROLLER_DATA.\n");
            logger_debug("Receiving data packet...\n");

            uint32_t bytes_received = 0;
            uint32_t tmp_bytes_received;

            while (bytes_received < pkt_size) {
                tmp_bytes_received = recv(openrgb_socket, response + bytes_received, pkt_size - bytes_received, 0);
                if (tmp_bytes_received <= 0) {
                    logger_debug("Error: Failed to receive data. Received %d bytes so far.\n", bytes_received);
                    return NULL;
                }
                bytes_received += tmp_bytes_received;
            }
            logger_debug("Received %d bytes. Starting parsing controller data...\n", bytes_received);
            struct openrgb_controller_data result;
            uint32_t offset = 0;
            memcpy(&result.data_size, response, 4);
            offset += 4;
            logger_debug("Data size: %d\n", result.data_size);
            memcpy(&result.type, response + offset, 4);
            offset += 4;
            logger_debug("Type: %d\n", result.type);
            memcpy(&result.name_len, response + offset, 2);
            offset += 2;
            logger_debug("Name length: %d\n", result.name_len);
            result.name = malloc(result.name_len);
            strcpy(result.name, response + offset);
            offset += result.name_len;
            logger_debug("Parsed name by bytes:\n");
            for (int i = 0; i < result.name_len; i++) {
                logger_debug("%x ", result.name[i]);
            }
            logger_debug("\nName: %s\n", result.name);

            if (openrgb_using_version > 1) {
                memcpy(&result.vendor_len, response + offset, 2);
                offset += 2;
                result.vendor = malloc(result.vendor_len);
                strcpy(result.vendor, response + offset);
                offset += result.vendor_len;
                logger_debug("Vendor: %s, length: %d\n", result.vendor, result.vendor_len);
            }

            memcpy(&result.description_len, response + offset, 2);
            offset += 2;
            result.description = malloc(result.description_len);
            strcpy(result.description, response + offset);
            offset += result.description_len;
            logger_debug("Description: %s, length: %d\n", result.description, result.description_len);

            memcpy(&result.version_len, response + offset, 2);
            offset += 2;
            result.version = malloc(result.version_len);
            strcpy(result.version, response + offset);
            offset += result.version_len;
            logger_debug("Version: %s, length: %d\n", result.version, result.version_len);

            memcpy(&result.serial_len, response + offset, 2);
            offset += 2;
            result.serial = malloc(result.serial_len);
            strcpy(result.serial, response + offset);
            offset += result.serial_len;
            logger_debug("Serial: %s, length: %d\n", result.serial, result.serial_len);

            memcpy(&result.location_len, response + offset, 2);
            offset += 2;
            result.location = malloc(result.location_len);
            strcpy(result.location, response + offset);
            offset += result.location_len;
            logger_debug("Location size: %d, Location: %s\n", result.location_len, result.location);

            memcpy(&result.num_modes, response + offset, 2);
            offset += 2;
            memcpy(&result.active_mode, response + offset, 4);
            offset += 4;
            logger_debug("Modes count: %d, active mode: %d\n", result.num_modes, result.active_mode);

            // parsing modes..
            struct openrgb_mode_data *mode_data = malloc(sizeof(struct openrgb_mode_data) * result.num_modes);
            for (int mode = 0; mode < result.num_modes; mode++) {
                logger_debug("Parsing mode #%d of %d\n", mode + 1, result.num_modes);
                memcpy(&mode_data[mode].mode_name_len, response + offset, 2);
                offset += 2;
                mode_data[mode].mode_name = malloc(mode_data[mode].mode_name_len);
                memcpy(mode_data[mode].mode_name, response + offset, mode_data[mode].mode_name_len);
                offset += mode_data[mode].mode_name_len;
                logger_debug("Mode name: %s\n", mode_data[mode].mode_name);

                memcpy(&mode_data[mode].mode_value, response + offset, 4);
                offset += 4;
                memcpy(&mode_data[mode].mode_flags, response + offset, 4);
                offset += 4;
                memcpy(&mode_data[mode].mode_speed_min, response + offset, 4);
                offset += 4;
                memcpy(&mode_data[mode].mode_speed_max, response + offset, 4);
                offset += 4;
                if (openrgb_using_version >= 3) {
                    memcpy(&mode_data[mode].mode_brightness_min, response + offset, 4);
                    offset += 4;
                    memcpy(&mode_data[mode].mode_brightness_max, response + offset, 4);
                    offset += 4;
                }
                memcpy(&mode_data[mode].mode_colors_min, response + offset, 4);
                offset += 4;
                memcpy(&mode_data[mode].mode_colors_max, response + offset, 4);
                offset += 4;
                memcpy(&mode_data[mode].mode_speed, response + offset, 4);
                offset += 4;
                if (openrgb_using_version >= 3) {
                    memcpy(&mode_data[mode].mode_brightness, response + offset, 4);
                    offset += 4;
                }
                memcpy(&mode_data[mode].mode_direction, response + offset, 4);
                offset += 4;
                memcpy(&mode_data[mode].mode_color_mode, response + offset, 4);
                offset += 4;
                memcpy(&mode_data[mode].mode_num_colors, response + offset, 2);
                offset += 2;
                logger_debug("Number of colors: %d\n", mode_data[mode].mode_num_colors);
                mode_data[mode].mode_colors = malloc(4 * mode_data[mode].mode_num_colors);
                memcpy(&mode_data[mode].mode_colors, response + offset, 4 * mode_data[mode].mode_num_colors);
                offset += 4 * mode_data[mode].mode_num_colors;
            }
            result.modes = mode_data;
            // parsing zones..........
            memcpy(&result.num_zones, response + offset, 2);
            offset += 2;
            logger_debug("Zones number: %d", result.num_zones);
            struct openrgb_zone_data *zone_data = malloc(sizeof(struct openrgb_zone_data) * result.num_zones);
            for (int zone = 0; zone < result.num_zones; zone++) {
                logger_debug("Parsing zone #%d of %d\n", zone + 1, result.num_zones);
                memcpy(&zone_data[zone].zone_name_len, response + offset, 2);
                offset += 2;
                zone_data[zone].zone_name = malloc(zone_data[zone].zone_name_len);
                memcpy(zone_data[zone].zone_name, response + offset, zone_data[zone].zone_name_len);
                offset += zone_data[zone].zone_name_len;
                logger_debug("Zone name: %s\n", zone_data[zone].zone_name);

                memcpy(&zone_data[zone].zone_type, response + offset, 4);
                offset += 4;

                memcpy(&zone_data[zone].zone_leds_min, response + offset, 4);
                offset += 4;
                memcpy(&zone_data[zone].zone_leds_max, response + offset, 4);
                offset += 4;

                memcpy(&zone_data[zone].zone_leds_count, response + offset, 4);
                offset += 4;

                memcpy(&zone_data[zone].zone_matrix_len, response + offset, 2);
                offset += 2;

                if (zone_data[zone].zone_matrix_len > 0) {
                    logger_debug("Zone matrix parsing not supported, skipping!\n");
                    // need to calculate offset...
                    // memcpy(&zone_data[zone].zone_matrix_height, response_data + offset, 4);
                    offset += 4;
                    // memcpy(&zone_data[zone].zone_matrix_width, response_data + offset, 4);
                    offset += 4;

                    // there could be zone_matrix_data...
                    offset += zone_data[zone].zone_matrix_len - 8;
                }
                offset += 2; // why?
            }
            result.zones = zone_data;

            // parsing leds
            memcpy(&result.num_leds, response + offset, 2);
            offset += 2;
            struct openrgb_led_data *led_data = malloc(sizeof(struct openrgb_led_data) * result.num_leds);
            for (int led = 0; led < result.num_leds; led++) {
                logger_debug("Parsing led #%d of %d\n", led, result.num_leds);
                memcpy(&led_data[led].led_name_len, response + offset, 2);
                offset += 2;
                led_data[led].led_name = malloc(led_data[led].led_name_len);
                memcpy(led_data[led].led_name, response + offset, led_data[led].led_name_len);
                offset += led_data[led].led_name_len;
                logger_debug("Led name: %s", led_data[led].led_name);
                memcpy(&led_data[led].led_value, response + offset, 4);
                offset += 4;
                logger_debug("Led value: %d", led_data[led].led_value);
            }
            result.leds = led_data;

            memcpy(&result.num_colors, response + offset, 2);
            offset += 2;
            for (int color = 0; color < result.num_colors; color++) {
                logger_debug("Parsing color #%d of %d\n", color, result.num_colors);
                uint32_t parsed_color = 0;
                memcpy(&parsed_color, response + offset, 4);
                logger_debug("Parsed color: %x\n", parsed_color);
            }
            openrgb_controllers[pkt_dev_idx] = result;
            if (pkt_dev_idx + 1 == openrgb_devices_num)
                openrgb_parsed_all_devices = 1;
        }
        case OPENRGB_NET_PACKET_ID_REQUEST_PROTOCOL_VERSION: {
            if (openrgb_using_version != -1)
                break;
            logger_debug("NET_PACKET_ID_REQUEST_PROTOCOL_VERSION");
            logger_debug("Receiving data packet...");

            uint32_t bytes_received = 0;
            uint32_t tmp_bytes_received;

            while (bytes_received < pkt_size) {
                tmp_bytes_received = recv(openrgb_socket, response + bytes_received, pkt_size - bytes_received, 0);
                if (tmp_bytes_received <= 0) {
                    logger_debug("Error: Failed to receive data. Received %d bytes so far.", bytes_received);
                    return NULL;
                }
                bytes_received += tmp_bytes_received;
            }
            logger_debug("Received %d bytes.", bytes_received);
            uint32_t openrgb_version = 0;
            memcpy(&openrgb_version, response, 4);
            openrgb_using_version =
                openrgb_version <= OPENRGB_SUPPORTED_VERSION ? openrgb_version : OPENRGB_SUPPORTED_VERSION;
            logger_debug("\nOpenRGB Server's Version: %d, Client max supported version: %d, Using version: %d\n",
                         openrgb_version, OPENRGB_SUPPORTED_VERSION, openrgb_using_version);

            break;
        }
        case OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT: {
            logger_debug("OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT");
            logger_debug("Receiving data packet...");

            uint32_t bytes_received = 0;
            uint32_t tmp_bytes_received;

            while (bytes_received < pkt_size) {
                tmp_bytes_received = recv(openrgb_socket, response + bytes_received, pkt_size - bytes_received, 0);
                if (tmp_bytes_received <= 0) {
                    logger_debug("Error: Failed to receive data. Received %d bytes so far.", bytes_received);
                    return NULL;
                }
                bytes_received += tmp_bytes_received;
            }
            logger_debug("Received %d bytes.", bytes_received);
            memcpy(&openrgb_devices_num, response, 4);
            logger_debug("Got OpenRGB devices: %d", openrgb_devices_num);

            break;
        }
        }
    }

    return NULL;
}

void openrgb_shutdown() {
    logger_debug("Stopping OpenRGB recv thread...");
    openrgb_stop_server = 1;
    pthread_join(openrgb_recv_thread_id, NULL);
    logger_debug("Releasing memory, allocated for OpenRGB");
    for (int i = 0; i < openrgb_devices_num; i++) {
        if (openrgb_controllers[i].name) {
            free(openrgb_controllers[i].name);
        }
        if (openrgb_controllers[i].vendor) {
            free(openrgb_controllers[i].vendor);
        }
        if (openrgb_controllers[i].description) {
            free(openrgb_controllers[i].description);
        }
        if (openrgb_controllers[i].version) {
            free(openrgb_controllers[i].version);
        }
        if (openrgb_controllers[i].serial) {
            free(openrgb_controllers[i].serial);
        }
        if (openrgb_controllers[i].location) {
            free(openrgb_controllers[i].location);
        }

        if (openrgb_controllers[i].modes) {
            if (openrgb_controllers[i].modes->mode_name)
                free(openrgb_controllers[i].modes->mode_name);
            if (openrgb_controllers[i].modes->mode_colors)
                free(openrgb_controllers[i].modes->mode_colors);
            free(openrgb_controllers[i].modes);
        }

        if (openrgb_controllers[i].zones) {
            if (openrgb_controllers[i].zones->zone_name)
                free(openrgb_controllers[i].zones->zone_name);
            if (openrgb_controllers[i].zones->zone_matrix_data)
                free(openrgb_controllers[i].zones->zone_matrix_data);
            free(openrgb_controllers[i].zones);
        }

        if (openrgb_controllers[i].leds) {
            if (openrgb_controllers[i].leds->led_name)
                free(openrgb_controllers[i].leds->led_name);
            free(openrgb_controllers[i].leds);
        }
    }
    if (openrgb_controllers)
        free(openrgb_controllers);

    for (int i = 0; i < openrgb_using_devices_num; i++) {
        if (openrgb_devices_to_change[i].name)
            free(openrgb_devices_to_change[i].name);
    }
    free(openrgb_devices_to_change);
}
