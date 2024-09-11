#include "openrgb.h"
#include "../server/server.h"
#include <arpa/inet.h>
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
uint32_t openrgb_using_version = 0;
pthread_t recv_thread_id;
uint8_t suspend_server = 0;

uint8_t *generate_packet(uint32_t pkt_dev_idx, uint32_t pkt_id, uint32_t pkg_size, const uint8_t *data) {
    printf("Generating OpenRGB header with dev_idx: %d, pkt_id: %d, pkg_size: %d\n", pkt_dev_idx, pkt_id, pkg_size);
    uint8_t *header = malloc(16 + pkg_size);
    header[0] = 'O';
    header[1] = 'R';
    header[2] = 'G';
    header[3] = 'B';

    memcpy((header + 4), &pkt_dev_idx, 4);
    memcpy((header + 8), &pkt_id, 4);
    memcpy((header + 12), &pkg_size, 4);
    for (int i = 0; i < pkg_size; i++) {
        header[i + 16] = data[i];
    }

    printf("OpenRGB: Generated header\n");
    for (int i = 0; i < 16 + pkg_size; i++) {
        printf("%x ", header[i]);
    }
    printf("\n");
    return header;
}

void openrgb_connect() {
    // connects to OpenRGB server, negotiates OpenSDK version and listens for responses
    struct sockaddr_in server_addr;
    openrgb_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (openrgb_socket < 0) {
        perror("Failed to create socket");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6742);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid OpenRGB server's IP address or IP address not supported");
        close(openrgb_socket);
        return;
    }

    if (connect(openrgb_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return;
    }

    // at this state we successfully connected to OpenRGB server, negotiating version
    uint8_t version[4];
    uint32_t number = OPENRGB_SUPPORTED_VERSION;
    memcpy(version, &number, 4);
    uint8_t *header = generate_packet(0, OPENRGB_NET_PACKET_ID_REQUEST_PROTOCOL_VERSION, 4, version);
    send(openrgb_socket, header, 20, 0);
    uint8_t response[16];
    int recv_size = recv(openrgb_socket, response, sizeof(response), 0);
    if (recv_size < 0) {
        perror("Failed to receive data");
        return;
    }

    if (recv_size == 0) {
        printf("Connection closed by peer\n");
        return;
    }

    printf("Received %d bytes from OpenRGB:\n", recv_size);
    for (int i = 0; i < recv_size; i++) {
        printf("%x ", response[i]);
        if (response[i] != header[i] && i < 12) {
            printf("Received unwanted package! Aborting.");
            return;
        }
    }
    printf("\n");
    printf("Receiving another %d bytes from OpenRGB...\n", response[12]);
    uint8_t response_data[response[12]];
    recv_size = recv(openrgb_socket, response_data, sizeof(response_data), 0);
    printf("Received %d bytes from OpenRGB:\n", recv_size);
    for (int i = 0; i < response[12]; i++) {
        printf("%x ", response_data[i]);
    }
    uint32_t openrgb_version = 0;
    memcpy(&openrgb_version, response_data, 4);
    openrgb_using_version = openrgb_version <= OPENRGB_SUPPORTED_VERSION ? openrgb_version : OPENRGB_SUPPORTED_VERSION;
    printf("\nOpenRGB Server's Version: %d, Client max supported version: %d, Using version: %d\n", openrgb_version,
           OPENRGB_SUPPORTED_VERSION, openrgb_using_version);

    free(header);
    printf("Setting client's name\n");

    header = generate_packet(0, OPENRGB_NET_PACKET_ID_SET_CLIENT_NAME, strlen("piled v4") + 1, "PiLED v4");
    if (send(openrgb_socket, header, 26, 0) < 0) {
        perror("Failed to send request to set client name");
        close(openrgb_socket);
        free(header);
        return;
    }

    printf("Started listening for responses...\n");

    if (pthread_create(&recv_thread_id, NULL, recv_thread, NULL) != 0) {
        perror("Failed to create receive thread");
        close(openrgb_socket);
        return;
    }

    request_controller_count();
    request_controller_data(2);

    pthread_join(recv_thread_id, NULL);

    free(header);
}

uint32_t request_controller_count() {
    // stopping listening for responses
    suspend_server = 1;
    pthread_join(recv_thread_id, NULL);
    printf("Requesting controller count");
    uint8_t *header = generate_packet(0, OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT, 0, NULL);
    if (send(openrgb_socket, header, 20, 0) < 0) {
        perror("Failed to send data");
        close(openrgb_socket);
        free(header);
    }
    uint8_t response[16];
    int recv_size = recv(openrgb_socket, response, sizeof(response), 0);
    printf("Received %d bytes from OpenRGB:\n", recv_size);
    for (int i = 0; i < recv_size; i++) {
        printf("%x ", response[i]);
        if (response[i] != header[i] && i < 12) {
            printf("Received unwanted package! Aborting.");
            suspend_server = 0;
            pthread_create(&recv_thread_id, NULL, recv_thread, NULL);
            return -1;
        }
    }
    printf("\n");
    recv_size = recv(openrgb_socket, response, sizeof(response), 0);
    printf("Received %d bytes with device count from OpenRGB:\n", recv_size);
    for (int i = 0; i < recv_size; i++) {
        printf("%x ", response[i]);
    }
    uint32_t device_count = 0;
    memcpy(&device_count, response, 4);
    printf("\nGot device count: %d\n", device_count);
    suspend_server = 0;
    pthread_create(&recv_thread_id, NULL, recv_thread, NULL);
    return device_count;
}

void *recv_thread(void *arg) {
    uint8_t response[1024];
    int recv_size;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    if (setsockopt(openrgb_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Failed to set socket timeout");
    }

    while (!stop_server && !suspend_server) {
        recv_size = recv(openrgb_socket, response, sizeof(response), 0);
        if (recv_size < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;
            }
            perror("Failed to receive data");
            break;
        }

        if (recv_size == 0) {
            printf("Connection closed by peer\n");
            break;
        }

        printf("Received %d bytes from OpenRGB:\n", recv_size);
        for (int i = 0; i < recv_size; i++) {
            printf("%x ", response[i]);
        }
        printf("\n");
    }

    return NULL;
}

struct openrgb_controller_data request_controller_data(uint32_t pkt_dev_idx) {
    // requests controller data and parses result
    // why we negotiated version,
    // in OpenRGB SDK Server tab there is printed "4" protocol version, but responses are as for 0 version?
    suspend_server = 1;
    pthread_join(recv_thread_id, NULL);
    printf("Requesting controller #%d data\n", pkt_dev_idx);
    uint8_t *header = generate_packet(pkt_dev_idx, OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_DATA, 0, NULL);
    if (send(openrgb_socket, header, 20, 0) < 0) {
        perror("Failed to send data");
        close(openrgb_socket);
        free(header);
    }
    uint8_t response[20];
    int recv_size = recv(openrgb_socket, response, sizeof(response), 0);
    printf("Received %d bytes from OpenRGB:\n", recv_size);
    for (int i = 0; i < recv_size; i++) {
        printf("%x ", response[i]);
        if (response[i] != header[i] && i < 12) {
            printf("Received unwanted package! Aborting.");
            suspend_server = 0;
            pthread_create(&recv_thread_id, NULL, recv_thread, NULL);
            // return -1;
        }
    }
    printf("\n");
    uint32_t response_data_size = 0;
    memcpy(&response_data_size, &response[12], 4);
    uint8_t response_data[response_data_size];
    recv_size = recv(openrgb_socket, response_data, response_data_size, 0);
    printf("Received %d bytes with device data from OpenRGB. Response data size: %d\n:\n", recv_size,
           response_data_size);
    for (int i = 0; i < recv_size; i++) {
        printf("%x ", response_data[i]);
    }
    printf("\nStarting parsing.........\n");

    struct openrgb_controller_data result;
    uint32_t offset = 0;
    memcpy(&result.data_size, response_data, 4);
    offset += 4;
    printf("Data size: %d\n", result.data_size);
    memcpy(&result.type, response_data + offset, 4);
    offset += 4;
    printf("Type: %d\n", result.type);
    memcpy(&result.name_len, response_data + offset, 2);
    offset += 2;
    printf("Name length: %d\n", result.name_len);
    result.name = malloc(result.name_len + 1);
    memcpy(result.name, response_data + offset, result.name_len);
    offset += result.name_len;
    printf("Parsed name by bytes:\n");
    for (int i = 0; i < result.name_len; i++) {
        printf("%x ", result.name[i]);
    }
    printf("\nName: %s\n", result.name);

    memcpy(&result.vendor_len, response_data + offset, 2);
    offset += 2;
    result.vendor = malloc(result.vendor_len);
    memcpy(result.vendor, response_data + offset, result.vendor_len);
    offset += result.vendor_len;
    printf("Vendor: %s\n", result.vendor);

    memcpy(&result.description_len, response_data + offset, 2);
    offset += 2;
    result.description = malloc(result.description_len);
    memcpy(result.description, response_data + offset, result.description_len);
    offset += result.description_len;
    printf("Description: %s\n", result.description);

    memcpy(&result.version_len, response_data + offset, 2);
    offset += 2;
    result.version = malloc(result.version_len);
    memcpy(result.version, response_data + offset, result.version_len);
    offset += result.version_len;
    printf("Version: %s\n", result.version);

    memcpy(&result.serial_len, response_data + offset, 2);
    offset += 2;
    result.serial = malloc(result.serial_len);
    memcpy(result.serial, response_data + offset, result.serial_len);
    offset += result.serial_len;
    printf("Serial: %s\n", result.serial);

    //  why location skipped in actual packets?
    //  memcpy(&result.location_len, response_data + offset, 2);
    //  offset += 2;
    //  result.location = malloc(result.location_len);
    //  memcpy(result.location, response_data + offset, result.location_len);
    //  offset += result.location_len;
    //  printf("Location size: %d, Location: %s\n", result.location_len, result.location);

    memcpy(&result.num_modes, response_data + offset, 2);
    offset += 2;
    memcpy(&result.active_mode, response_data + offset, 4);
    offset += 4;
    printf("Modes count: %d, active mode: %d\n", result.num_modes, result.active_mode);

    // parsing modes..
    struct openrgb_mode_data *mode_data = malloc(sizeof(struct openrgb_mode_data) * result.num_modes);
    for (int mode = 0; mode < result.num_modes; mode++) {
        printf("Parsing mode #%d of %d\n", mode + 1, result.num_modes);
        memcpy(&mode_data[mode].mode_name_len, response_data + offset, 2);
        offset += 2;
        mode_data[mode].mode_name = malloc(mode_data[mode].mode_name_len);
        memcpy(mode_data[mode].mode_name, response_data + offset, mode_data[mode].mode_name_len);
        offset += mode_data[mode].mode_name_len;
        printf("Mode name: %s\n", mode_data[mode].mode_name);

        memcpy(&mode_data[mode].mode_value, response_data + offset, 4);
        offset += 4;
        memcpy(&mode_data[mode].mode_flags, response_data + offset, 4);
        offset += 4;
        memcpy(&mode_data[mode].mode_speed_min, response_data + offset, 4);
        offset += 4;
        memcpy(&mode_data[mode].mode_speed_max, response_data + offset, 4);
        offset += 4;
        // if (OPENRGB_SUPPORTED_VERSION >= 3) {
        //     memcpy(&mode_data[mode].mode_brightness_min, response_data + offset, 4);
        //     offset += 4;
        //     memcpy(&mode_data[mode].mode_brightness_max, response_data + offset, 4);
        //     offset += 4;
        // }
        memcpy(&mode_data[mode].mode_colors_min, response_data + offset, 4);
        offset += 4;
        memcpy(&mode_data[mode].mode_colors_max, response_data + offset, 4);
        offset += 4;
        memcpy(&mode_data[mode].mode_speed, response_data + offset, 4);
        offset += 4;
        // if (OPENRGB_SUPPORTED_VERSION >= 3) {
        //     memcpy(&mode_data[mode].mode_brightness, response_data + offset, 4);
        //     offset += 4;
        // }
        memcpy(&mode_data[mode].mode_direction, response_data + offset, 4);
        offset += 4;
        memcpy(&mode_data[mode].mode_color_mode, response_data + offset, 4);
        offset += 4;
        memcpy(&mode_data[mode].mode_num_colors, response_data + offset, 2);
        offset += 2;
        printf("Number of colors: %d\n", mode_data[mode].mode_num_colors);
        mode_data[mode].mode_colors = malloc(4 * mode_data[mode].mode_num_colors);
        memcpy(&mode_data[mode].mode_colors, response_data + offset, 4 * mode_data[mode].mode_num_colors);
        offset += 4 * mode_data[mode].mode_num_colors;
    }
    // parsing zones..........
    memcpy(&result.num_zones, response_data + offset, 2);
    offset += 2;
    struct openrgb_zone_data *zone_data = malloc(sizeof(struct openrgb_zone_data) * result.num_zones);
    for (int zone = 0; zone < result.num_zones; zone++) {
        printf("Parsing zone #%d of %d\n", zone, result.num_zones);
        memcpy(&zone_data[zone].zone_name_len, response_data + offset, 2);
        offset += 2;
        zone_data[zone].zone_name = malloc(zone_data[zone].zone_name_len);
        memcpy(zone_data[zone].zone_name, response_data + offset, zone_data[zone].zone_name_len);
        offset += zone_data[zone].zone_name_len;
        printf("Zone name: %s\n", zone_data[zone].zone_name);

        memcpy(&zone_data[zone].zone_type, response_data + offset, 4);
        offset += 4;

        memcpy(&zone_data[zone].zone_leds_min, response_data + offset, 4);
        offset += 4;
        memcpy(&zone_data[zone].zone_leds_max, response_data + offset, 4);
        offset += 4;

        memcpy(&zone_data[zone].zone_leds_count, response_data + offset, 4);
        offset += 4;

        memcpy(&zone_data[zone].zone_matrix_len, response_data + offset, 2);
        offset += 2;

        if (zone_data[zone].zone_matrix_len > 0) {
            printf("Zone matrix parsing not supported, skipping!\n");
            // need to calculate offset...
            // memcpy(&zone_data[zone].zone_matrix_height, response_data + offset, 4);
            offset += 4;
            // memcpy(&zone_data[zone].zone_matrix_width, response_data + offset, 4);
            offset += 4;

            // there could be zone_matrix_data...
            offset += zone_data[zone].zone_matrix_len - 8;
        }
    }

    // parsing leds
    memcpy(&result.num_leds, response_data + offset, 2);
    offset += 2;
    struct openrgb_led_data *led_data = malloc(sizeof(struct openrgb_led_data) * result.num_leds);
    for (int led = 0; led < result.num_leds; led++) {
        printf("Parsing led #%d of %d\n", led, result.num_leds);
        memcpy(&led_data[led].led_name_len, response_data + offset, 2);
        offset += 2;
        led_data[led].led_name = malloc(led_data[led].led_name_len);
        memcpy(led_data[led].led_name, response_data + offset, led_data[led].led_name_len);
        offset += led_data[led].led_name_len;
        memcpy(&led_data[led].led_value, response_data + offset, 4);
        offset += 4;
    }

    memcpy(&result.num_colors, response_data + offset, 2);
    offset += 2;
    for (int color = 0; color < result.num_colors; color++) {
        printf("Parsing color #%d of %d\n", color, result.num_colors);
        uint32_t parsed_color = 0;
        memcpy(&parsed_color, response_data + offset, 4);
        printf("Parsed color: %x\n", parsed_color);
    }

    suspend_server = 0;
    pthread_create(&recv_thread_id, NULL, recv_thread, NULL);
}

void free_openrgb_controller_data(struct openrgb_controller_data *data) {
    if (data->name) {
        free(data->name);
    }
    if (data->vendor) {
        free(data->vendor);
    }
    if (data->description) {
        free(data->description);
    }
    if (data->version) {
        free(data->version);
    }
    if (data->serial) {
        free(data->serial);
    }
    if (data->location) {
        free(data->location);
    }
}
