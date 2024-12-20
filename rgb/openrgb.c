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
pthread_t openrgb_recv_thread_id, openrgb_reconnect_thread_id;
pthread_mutex_t openrgb_send_mutex;
int8_t openrgb_using_version = -1;
int32_t openrgb_devices_num = -1;
int32_t openrgb_using_devices_num = 0;
int8_t openrgb_parsed_all_devices = -1;
struct openrgb_controller_data *openrgb_controllers;
volatile sig_atomic_t openrgb_stop_server = 0, openrgb_needs_reinit = 0, openrgb_exit = 0;
struct openrgb_device *openrgb_devices_to_change;

void openrgb_init_header(uint8_t *header, uint32_t pkt_dev_idx, uint32_t pkt_id, uint32_t pkg_size) {
    // adding magick
    header[0] = 'O';
    header[1] = 'R';
    header[2] = 'G';
    header[3] = 'B';
    memcpy((header + 4), &pkt_dev_idx, 4);
    memcpy((header + 8), &pkt_id, 4);
    memcpy((header + 12), &pkg_size, 4);
#ifdef DEBUG
    printf("OpenRGB: Generated header\n");
    for (int i = 0; i < 16; i++) {
        printf("%x ", header[i]);
    }
    printf("\n");
#endif
}

void *openrgb_init() {
    openrgb_stop_server = 0;
    openrgb_needs_reinit = 0;
    openrgb_using_devices_num = 0;
    openrgb_devices_num = -1;
    openrgb_using_version = -1;
    openrgb_parsed_all_devices = -1;
    logger(OPENRGB, "Initializing OpenRGB!");
    // connects to OpenRGB server, negotiates OpenRGB's SDK version and listens for responses
    if (pthread_mutex_init(&openrgb_send_mutex, NULL) != 0) {
        logger(OPENRGB, "Mutex init failed\n");
        return NULL;
    }

    struct sockaddr_in server_addr;
    openrgb_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (openrgb_socket < 0) {
        logger(OPENRGB, "Failed to create socket");
        return NULL;
    }
    logger(OPENRGB, "Created socket.");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(OPENRGB_PORT);

    uint8_t connected = 0;
    while (!connected && !openrgb_stop_server) {
        if (inet_pton(AF_INET, OPENRGB_SERVER, &server_addr.sin_addr) <= 0) {
            logger(OPENRGB, "Invalid OpenRGB server's IP address or IP address not supported");
            close(openrgb_socket);
            sleep(30);
            continue;
        }

        if (connect(openrgb_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            logger(OPENRGB, "Connection failed, waiting 30 seconds and trying again...");
            sleep(30);
            continue;
        } else {
            connected = 1;
        }
    }

    logger(OPENRGB, "Connected to OpenRGB Server.");

    // at this state we successfully connected to OpenRGB server, starting recv thread

    if (pthread_create(&openrgb_recv_thread_id, NULL, openrgb_recv_thread, NULL) != 0) {
        logger(OPENRGB, "Failed to create receive thread");
        close(openrgb_socket);
        return NULL;
    }

    // now all responses by openrgb will be received by recv thread.
    // requesting version.
    openrgb_request_protocol_version();
    logger_debug(OPENRGB, "Requested protocol version, waiting...");
    uint8_t timeout_counter = 0;
    while (openrgb_using_version == -1) {
        logger_debug(OPENRGB, "Waiting... current openrgb version: %d", openrgb_using_version);
        usleep(100000);
        timeout_counter++;
        if (timeout_counter > 10) {
            // if no protocol version received within 1s, assuming that server does not
            // supports protocol versioning and working at version 0
            openrgb_using_version = 0;
            break;
        }
    }
    logger_debug(OPENRGB, "OpenRGB negotiated version is %d!", openrgb_using_version);

    logger_debug(OPENRGB, "Setting client's name");
    openrgb_set_client_name();

    logger_debug(OPENRGB, "Getting current devices number");
    timeout_counter = 0;
    openrgb_request_controller_count();
    while (openrgb_devices_num == -1) {
        logger_debug(OPENRGB, "Waiting... current openrgb version: %d", openrgb_using_version);
        usleep(100000);
        timeout_counter++;
        if (timeout_counter > 20) {
            logger_debug(OPENRGB, "Error! Can't get devices number!");
            openrgb_devices_num = 0;
            return NULL;
            break;
        }
    }
    logger_debug(OPENRGB, "Got %d devices count", openrgb_devices_num);

    openrgb_controllers = malloc(openrgb_devices_num * sizeof(struct openrgb_controller_data));

    for (int device = 0; device < openrgb_devices_num; device++) {
        logger_debug(OPENRGB, "Getting device %d data...", device);
        openrgb_request_controller_data(device);
    }

    logger_debug(OPENRGB, "Requested all controller data.");
    while (openrgb_parsed_all_devices == -1) {
        logger_debug(OPENRGB, "Waiting for all devices...");
        usleep(200000);
        timeout_counter++;
        if (timeout_counter > 20) {
            logger_debug(OPENRGB, "Error! Can't parse all device or timeout!");
            break;
        }
    }

#ifndef ORGBCONFIGURATOR
    logger_debug(OPENRGB, "Parsing device preferences config...");
    parse_openrgb_config_devices("/etc/piled/openrgb_config");
#endif

    pthread_create(&openrgb_reconnect_thread_id, NULL, openrgb_reconnect_thread, NULL);
    return NULL;
}

void openrgb_request_protocol_version() {
    logger_debug(OPENRGB, "Requesting protocol version.");
    uint8_t header[16];
    openrgb_init_header(header, 0, OPENRGB_NET_PACKET_ID_REQUEST_PROTOCOL_VERSION, 4);
    uint32_t version = OPENRGB_SUPPORTED_VERSION;
    pthread_mutex_lock(&openrgb_send_mutex);
#ifdef DEBUG
    printf("Sending header:\n");
    for (int i = 0; i < 16; i++) {
        printf("%x ", header[i]);
    }
    printf("\n");
#endif
    send(openrgb_socket, header, 16, MSG_NOSIGNAL);
#ifdef DEBUG
    printf("Sending version: %d\n", version);
#endif
    send(openrgb_socket, &version, 4, MSG_NOSIGNAL);
    pthread_mutex_unlock(&openrgb_send_mutex);
}

void openrgb_set_client_name() {
    uint8_t *name = malloc(strlen("piled vx"));
    strcpy((char *)name, "PiLED v");
    name[7] = PILED_VERSION + '0';
    uint8_t header[16];
    openrgb_init_header(header, 0, OPENRGB_NET_PACKET_ID_SET_CLIENT_NAME, strlen("piled vx"));
    pthread_mutex_lock(&openrgb_send_mutex);
    send(openrgb_socket, header, 16, MSG_NOSIGNAL);
    send(openrgb_socket, name, strlen("piled vx"), MSG_NOSIGNAL);
    pthread_mutex_unlock(&openrgb_send_mutex);
    free(name);
}

void openrgb_request_controller_count() {
    logger_debug(OPENRGB, "Requesting controller count");
    uint8_t header[16];
    openrgb_init_header(header, 0, OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT, 0);
    pthread_mutex_lock(&openrgb_send_mutex);
    send(openrgb_socket, header, 16, MSG_NOSIGNAL);
    pthread_mutex_unlock(&openrgb_send_mutex);
}

void openrgb_request_controller_data(uint32_t pkt_dev_idx) {
    logger_debug(OPENRGB, "Requesting controller data");
    uint8_t header[16];
    openrgb_init_header(header, pkt_dev_idx, OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_DATA, 4);
    pthread_mutex_lock(&openrgb_send_mutex);
    send(openrgb_socket, header, 16, MSG_NOSIGNAL);
    send(openrgb_socket, &openrgb_using_version, 4, MSG_NOSIGNAL);
    pthread_mutex_unlock(&openrgb_send_mutex);
}

void openrgb_request_update_leds(uint32_t pkt_dev_idx, struct Color color) {
    logger_debug(OPENRGB, "Setting controller #%d color", pkt_dev_idx);

    uint32_t packet_size = 4 +                                            // data_size
                           2 +                                            // num_colors
                           4 * openrgb_controllers[pkt_dev_idx].num_leds; // led_color

    uint8_t header[16];
    openrgb_init_header(header, pkt_dev_idx, OPENRGB_NET_PACKET_ID_RGBCONTROLLER_UPDATELEDS, packet_size);

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
        logger_debug(OPENRGB, "red: %x, green: %x, blue: %x\nColor data: %02x", color.RED, color.GREEN, color.BLUE,
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
    send(openrgb_socket, header, 16, MSG_NOSIGNAL);
    send(openrgb_socket, packet, packet_size, MSG_NOSIGNAL);
    pthread_mutex_unlock(&openrgb_send_mutex);
}

void openrgb_set_color_on_devices(struct Color color) {
    for (uint16_t device = 0; device < openrgb_using_devices_num; device++) {
        openrgb_request_update_leds(openrgb_devices_to_change[device].device_id, color);
    }
}

void *openrgb_recv_thread(void *arg) {
    logger(OPENRGB, "Started OpenRGB receive thread!");
    uint8_t header_recv[16];
    int recv_size;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    if (setsockopt(openrgb_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        logger_debug(OPENRGB, "Failed to set socket timeout");
    }

    while (!openrgb_stop_server) {
        recv_size = recv(openrgb_socket, header_recv, sizeof(header_recv), 0);
        if (recv_size < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;
            }
            logger_debug(OPENRGB, "Failed to receive data");
            break;
        }

        if (recv_size == 0) {
            logger(OPENRGB, "Connection closed by peer");
            openrgb_needs_reinit = 1;
            break;
        }

        logger_debug(OPENRGB, "OpenRGB Parser: Starting parsing buffer with size %d", recv_size);
#ifdef DEBUG
        printf("Buffer:\n");
        for (int i = 0; i < recv_size; i++) {
            printf("%x ", header_recv[i]);
        }
        printf("\n");
#endif

        // сhecking for magick
        if (header_recv[0] != 'O' || header_recv[1] != 'R' || header_recv[2] != 'G' || header_recv[3] != 'B') {
            logger_debug(OPENRGB, "Magick wrong! Not a OpenRGB package!");
            return NULL;
        }

        uint32_t pkt_dev_idx = 0;
        memcpy(&pkt_dev_idx, header_recv + 4, 4);
        logger_debug(OPENRGB, "Got device index: %d", pkt_dev_idx);

        uint32_t pkt_id = 0;
        memcpy(&pkt_id, header_recv + 8, 4);
        logger_debug(OPENRGB, "Got packet id: %d", pkt_id);

        uint32_t pkt_size = 0;
        memcpy(&pkt_size, header_recv + 12, 4);
        logger_debug(OPENRGB, "Got packet size: %d", pkt_size);
        // entire header received, now receive the data
        uint8_t response[pkt_size];
        switch (pkt_id) {
        case OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_DATA: {
            logger_debug(OPENRGB, "NET_PACKET_ID_REQUEST_CONTROLLER_DATA.");
            logger_debug(OPENRGB, "Receiving data packet...");

            uint32_t bytes_received = 0;
            uint32_t tmp_bytes_received;

            while (bytes_received < pkt_size) {
                tmp_bytes_received = recv(openrgb_socket, response + bytes_received, pkt_size - bytes_received, 0);
                if (tmp_bytes_received <= 0) {
                    logger_debug(OPENRGB, "Error: Failed to receive data. Received %d bytes so far.", bytes_received);
                    return NULL;
                }
                bytes_received += tmp_bytes_received;
            }
            logger_debug(OPENRGB, "Received %d bytes. Starting parsing controller data...", bytes_received);
            struct openrgb_controller_data result;
            uint32_t offset = 0;
            result.data_size = 0;
            memcpy(&result.data_size, response, 4);
            offset += 4;
            result.type = 0;
            logger_debug(OPENRGB, "Data size: %d", result.data_size);
            memcpy(&result.type, response + offset, 4);
            offset += 4;
            result.name_len = 1; // initializing as 1 bc it will store '\0' anyway
            logger_debug(OPENRGB, "Type: %d", result.type);
            memcpy(&result.name_len, response + offset, 2);
            offset += 2;
            logger_debug(OPENRGB, "Name length: %d", result.name_len);
            result.name = malloc(result.name_len);
            strcpy((char *)result.name, (char *)response + offset);
            result.name[result.name_len - 1] = 0;
            offset += result.name_len;
            logger_debug(OPENRGB, "Parsed name by bytes:");
            for (int i = 0; i < result.name_len; i++) {
                logger_debug(OPENRGB, "%x ", result.name[i]);
            }
            logger_debug(OPENRGB, "\nName: %s", result.name);

            if (openrgb_using_version > 1) {
                result.version_len = 1;
                memcpy(&result.vendor_len, response + offset, 2);
                offset += 2;
                result.vendor = malloc(result.vendor_len);
                strcpy((char *)result.vendor, (char *)response + offset);
                result.vendor[result.vendor_len - 1] = 0;
                offset += result.vendor_len;
                logger_debug(OPENRGB, "Vendor: %s, length: %d", result.vendor, result.vendor_len);
            }
            result.description_len = 1;
            memcpy(&result.description_len, response + offset, 2);
            offset += 2;
            result.description = malloc(result.description_len);
            strcpy((char *)result.description, (char *)response + offset);
            result.description[result.description_len - 1] = 0;
            offset += result.description_len;
            logger_debug(OPENRGB, "Description: %s, length: %d", result.description, result.description_len);

            result.version_len = 1;
            memcpy(&result.version_len, response + offset, 2);
            offset += 2;
            result.version = malloc(result.version_len);
            strcpy((char *)result.version, (char *)response + offset);
            result.version[result.version_len - 1] = 0;
            offset += result.version_len;
            logger_debug(OPENRGB, "Version: %s, length: %d", result.version, result.version_len);

            result.serial_len = 1;
            memcpy(&result.serial_len, response + offset, 2);
            offset += 2;
            result.serial = malloc(result.serial_len);
            strcpy((char *)result.serial, (char *)response + offset);
            result.serial[result.serial_len - 1] = 0;
            offset += result.serial_len;
            logger_debug(OPENRGB, "Serial: %s, length: %d", result.serial, result.serial_len);

            result.location_len = 1;
            memcpy(&result.location_len, response + offset, 2);
            offset += 2;
            result.location = malloc(result.location_len);
            strcpy((char *)result.location, (char *)response + offset);
            result.location[result.location_len - 1] = 0;
            offset += result.location_len;
            logger_debug(OPENRGB, "Location size: %d, Location: %s", result.location_len, result.location);

            result.num_modes = 0;
            memcpy(&result.num_modes, response + offset, 2);
            offset += 2;
            result.active_mode = 0;
            memcpy(&result.active_mode, response + offset, 4);
            offset += 4;
            logger_debug(OPENRGB, "Modes count: %d, active mode: %d", result.num_modes, result.active_mode);

            // parsing modes..
            struct openrgb_mode_data *mode_data = malloc(sizeof(struct openrgb_mode_data) * result.num_modes);
            for (int mode = 0; mode < result.num_modes; mode++) {
                logger_debug(OPENRGB, "Parsing mode #%d of %d", mode + 1, result.num_modes);
                mode_data[mode].mode_name_len = 1;
                memcpy(&mode_data[mode].mode_name_len, response + offset, 2);
                offset += 2;
                mode_data[mode].mode_name = malloc(mode_data[mode].mode_name_len);
                memcpy(mode_data[mode].mode_name, response + offset, mode_data[mode].mode_name_len);
                mode_data[mode].mode_name[mode_data[mode].mode_name_len - 1] = 0;
                offset += mode_data[mode].mode_name_len;
                logger_debug(OPENRGB, "Mode name: %s", mode_data[mode].mode_name);

                mode_data[mode].mode_value = 0;
                memcpy(&mode_data[mode].mode_value, response + offset, 4);
                offset += 4;
                mode_data[mode].mode_flags = 0;
                memcpy(&mode_data[mode].mode_flags, response + offset, 4);
                offset += 4;
                mode_data[mode].mode_speed_min = 0;
                memcpy(&mode_data[mode].mode_speed_min, response + offset, 4);
                offset += 4;
                mode_data[mode].mode_speed_max = 0;
                memcpy(&mode_data[mode].mode_speed_max, response + offset, 4);
                offset += 4;
                if (openrgb_using_version >= 3) {
                    mode_data[mode].mode_brightness_min = 0;
                    memcpy(&mode_data[mode].mode_brightness_min, response + offset, 4);
                    offset += 4;
                    mode_data[mode].mode_brightness_max = 1;
                    memcpy(&mode_data[mode].mode_brightness_max, response + offset, 4);
                    offset += 4;
                }
                mode_data[mode].mode_colors_min = 0;
                memcpy(&mode_data[mode].mode_colors_min, response + offset, 4);
                offset += 4;
                mode_data[mode].mode_colors_max = 0;
                memcpy(&mode_data[mode].mode_colors_max, response + offset, 4);
                offset += 4;
                mode_data[mode].mode_speed = 0;
                memcpy(&mode_data[mode].mode_speed, response + offset, 4);
                offset += 4;
                if (openrgb_using_version >= 3) {
                    mode_data[mode].mode_brightness = 0;
                    memcpy(&mode_data[mode].mode_brightness, response + offset, 4);
                    offset += 4;
                }
                mode_data[mode].mode_direction = 0;
                memcpy(&mode_data[mode].mode_direction, response + offset, 4);
                offset += 4;
                mode_data[mode].mode_color_mode = 0;
                memcpy(&mode_data[mode].mode_color_mode, response + offset, 4);
                offset += 4;
                mode_data[mode].mode_num_colors = 0;
                memcpy(&mode_data[mode].mode_num_colors, response + offset, 2);
                offset += 2;
                logger_debug(OPENRGB, "Number of colors: %d", mode_data[mode].mode_num_colors);
                mode_data[mode].mode_colors = malloc(4 * mode_data[mode].mode_num_colors);
                memcpy(mode_data[mode].mode_colors, response + offset, 4 * mode_data[mode].mode_num_colors);
                offset += 4 * mode_data[mode].mode_num_colors;
            }
            result.modes = mode_data;
            // parsing zones..........
            result.num_zones = 0;
            memcpy(&result.num_zones, response + offset, 2);
            offset += 2;
            logger_debug(OPENRGB, "Zones number: %d", result.num_zones);
            struct openrgb_zone_data *zone_data = malloc(sizeof(struct openrgb_zone_data) * result.num_zones);
            for (int zone = 0; zone < result.num_zones; zone++) {
                logger_debug(OPENRGB, "Parsing zone #%d of %d", zone + 1, result.num_zones);
                zone_data[zone].zone_name_len = 1;
                memcpy(&zone_data[zone].zone_name_len, response + offset, 2);
                offset += 2;
                zone_data[zone].zone_name = malloc(zone_data[zone].zone_name_len);
                memcpy(zone_data[zone].zone_name, response + offset, zone_data[zone].zone_name_len);
                zone_data[zone].zone_name[zone_data[zone].zone_name_len - 1] = 0;
                offset += zone_data[zone].zone_name_len;
                logger_debug(OPENRGB, "Zone name: %s", zone_data[zone].zone_name);

                zone_data[zone].zone_type = 0;
                memcpy(&zone_data[zone].zone_type, response + offset, 4);
                offset += 4;

                zone_data[zone].zone_leds_min = 0;
                memcpy(&zone_data[zone].zone_leds_min, response + offset, 4);
                offset += 4;
                zone_data[zone].zone_leds_max = 0;
                memcpy(&zone_data[zone].zone_leds_max, response + offset, 4);
                offset += 4;

                zone_data[zone].zone_leds_count = 0;
                memcpy(&zone_data[zone].zone_leds_count, response + offset, 4);
                offset += 4;

                zone_data[zone].zone_matrix_len = 0;
                memcpy(&zone_data[zone].zone_matrix_len, response + offset, 2);
                offset += 2;

                if (zone_data[zone].zone_matrix_len > 0) {
                    logger_debug(OPENRGB, "Zone matrix parsing not supported, skipping!");
                    // need to calculate offset...
                    // memcpy(&zone_data[zone].zone_matrix_height, response_data + offset, 4);
                    offset += 4;
                    // memcpy(&zone_data[zone].zone_matrix_width, response_data + offset, 4);
                    offset += 4;

                    // there could be zone_matrix_data...
                    offset += zone_data[zone].zone_matrix_len - 8;
                    zone_data[zone].zone_matrix_data = NULL;
                }
                zone_data[zone].zone_matrix_data = NULL;
                offset += 2; // why?
            }
            result.zones = zone_data;

            // parsing leds
            result.num_leds = 0;
            memcpy(&result.num_leds, response + offset, 2);
            offset += 2;
            struct openrgb_led_data *led_data = malloc(sizeof(struct openrgb_led_data) * result.num_leds);
            for (int led = 0; led < result.num_leds; led++) {
                logger_debug(OPENRGB, "Parsing led #%d of %d", led, result.num_leds);
                led_data[led].led_name_len = 1;
                memcpy(&led_data[led].led_name_len, response + offset, 2);
                offset += 2;
                logger_debug(OPENRGB, "Led name len: %d", led_data[led].led_name_len);
                led_data[led].led_name = malloc(led_data[led].led_name_len);
                memcpy(led_data[led].led_name, response + offset, led_data[led].led_name_len);
                led_data[led].led_name[led_data[led].led_name_len - 1] = 0;
                offset += led_data[led].led_name_len;
                logger_debug(OPENRGB, "Led name: %s", led_data[led].led_name);
                led_data[led].led_value = 0;
                memcpy(&led_data[led].led_value, response + offset, 4);
                offset += 4;
                logger_debug(OPENRGB, "Led value: %d", led_data[led].led_value);
            }
            result.leds = led_data;

            memcpy(&result.num_colors, response + offset, 2);
            offset += 2;
            for (int color = 0; color < result.num_colors; color++) {
                logger_debug(OPENRGB, "Parsing color #%d of %d", color, result.num_colors);
                uint32_t parsed_color = 0;
                memcpy(&parsed_color, response + offset, 4);
                logger_debug(OPENRGB, "Parsed color: %x", parsed_color);
            }
            openrgb_controllers[pkt_dev_idx] = result;
            if (pkt_dev_idx + 1 == openrgb_devices_num)
                openrgb_parsed_all_devices = 1;
        }
        case OPENRGB_NET_PACKET_ID_REQUEST_PROTOCOL_VERSION: {
            if (openrgb_using_version != -1)
                break;
            logger_debug(OPENRGB, "NET_PACKET_ID_REQUEST_PROTOCOL_VERSION");
            logger_debug(OPENRGB, "Receiving data packet...");

            uint32_t bytes_received = 0;
            uint32_t tmp_bytes_received;

            while (bytes_received < pkt_size) {
                tmp_bytes_received = recv(openrgb_socket, response + bytes_received, pkt_size - bytes_received, 0);
                if (tmp_bytes_received <= 0) {
                    logger_debug(OPENRGB, "Error: Failed to receive data. Received %d bytes so far.", bytes_received);
                    return NULL;
                }
                bytes_received += tmp_bytes_received;
            }
            logger_debug(OPENRGB, "Received %d bytes.", bytes_received);
            uint32_t openrgb_version = 0;
            memcpy(&openrgb_version, response, 4);
            openrgb_using_version =
                openrgb_version <= OPENRGB_SUPPORTED_VERSION ? openrgb_version : OPENRGB_SUPPORTED_VERSION;
            logger_debug(OPENRGB, "OpenRGB Server's Version: %d, Client max supported version: %d, Using version: %d",
                         openrgb_version, OPENRGB_SUPPORTED_VERSION, openrgb_using_version);

            break;
        }
        case OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT: {
            logger_debug(OPENRGB, "OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT");
            logger_debug(OPENRGB, "Receiving data packet...");

            uint32_t bytes_received = 0;
            uint32_t tmp_bytes_received;

            while (bytes_received < pkt_size) {
                tmp_bytes_received = recv(openrgb_socket, response + bytes_received, pkt_size - bytes_received, 0);
                if (tmp_bytes_received <= 0) {
                    logger_debug(OPENRGB, "Error: Failed to receive data. Received %d bytes so far.", bytes_received);
                    return NULL;
                }
                bytes_received += tmp_bytes_received;
            }
            logger_debug(OPENRGB, "Received %d bytes.", bytes_received);
            memcpy(&openrgb_devices_num, response, 4);
            logger_debug(OPENRGB, "Got OpenRGB devices: %d", openrgb_devices_num);

            break;
        }
        }
    }

    return NULL;
}

void openrgb_shutdown() {
    logger(OPENRGB, "Stopping OpenRGB recv thread...");
    if (openrgb_recv_thread_id) {
        openrgb_stop_server = 1;
        pthread_join(openrgb_recv_thread_id, NULL);
    }

    if (openrgb_socket >= 0) {
        pthread_mutex_lock(&openrgb_send_mutex);
        close(openrgb_socket);
        pthread_mutex_unlock(&openrgb_send_mutex);
        openrgb_socket = -1;
    }

    if (openrgb_exit == 1) {
        openrgb_needs_reinit = 0;
        if (openrgb_reconnect_thread_id)
            pthread_join(openrgb_reconnect_thread_id, NULL);
    }

    logger(OPENRGB, "Releasing memory, allocated for OpenRGB");
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
            for (int mode = 0; mode < openrgb_controllers[i].num_modes; mode++) {
                if (openrgb_controllers[i].modes[mode].mode_name != NULL)
                    free(openrgb_controllers[i].modes[mode].mode_name);
                if (openrgb_controllers[i].modes[mode].mode_colors != NULL)
                    free(openrgb_controllers[i].modes[mode].mode_colors);
            }
        }
        free(openrgb_controllers[i].modes);

        if (openrgb_controllers[i].zones) {
            for (int zone = 0; zone < openrgb_controllers[i].num_zones; zone++) {
                if (openrgb_controllers[i].zones[zone].zone_name)
                    free(openrgb_controllers[i].zones[zone].zone_name);
                if (openrgb_controllers[i].zones[zone].zone_matrix_data != NULL)
                    free(openrgb_controllers[i].zones[zone].zone_matrix_data);
            }
        }
        free(openrgb_controllers[i].zones);

        if (openrgb_controllers[i].leds) {
            for (int led = 0; led < openrgb_controllers[i].num_leds; led++) {
                if (openrgb_controllers[i].leds[led].led_name)
                    free(openrgb_controllers[i].leds[led].led_name);
            }
        }
        free(openrgb_controllers[i].leds);
    }
    if (openrgb_controllers)
        free(openrgb_controllers);

    for (int i = 0; i < openrgb_using_devices_num; i++) {
        if (openrgb_devices_to_change[i].name)
            free(openrgb_devices_to_change[i].name);
    }
    free(openrgb_devices_to_change);
}

void *openrgb_reconnect_thread(void *arg) {
    logger(OPENRGB, "Started reconnect watcher thread...");
    while (!openrgb_stop_server) {  // if not shut down
        if (openrgb_needs_reinit) { // if connection was closed
            openrgb_needs_reinit = 0;

            logger(OPENRGB, "Connection to OpenRGB is closed, need to reinit OpenRGB.");
            openrgb_shutdown();

            usleep(500000);
            logger(OPENRGB, "Initializing OpenRGB again");
            openrgb_init();
        }
        usleep(100000);
    }
    logger(OPENRGB, "Reconnect watcher thread exiting.");
    return NULL;
}