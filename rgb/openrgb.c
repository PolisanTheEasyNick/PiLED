#include "openrgb.h"
#include "../server/server.h"
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
uint8_t openrgb_set_clientname = 0;

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
    for (int i = 0; i < 16 + pkg_size; i++) {
        printf("%x ", (*header)[i]);
    }
    printf("\n");
#endif
}

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

void openrgb_init() {
    logger("Initializing OpenRGB!");
    // connects to OpenRGB server, negotiates OpenRGB's SDK version and listens for responses
    if (pthread_mutex_init(&openrgb_send_mutex, NULL) != 0) {
        printf("Mutex init failed\n");
        return;
    }

    struct sockaddr_in server_addr;
    openrgb_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (openrgb_socket < 0) {
        perror("Failed to create socket");
        return;
    }
    logger("Created socket.");

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
    logger("Connected to OpenRGB Server.");

    // at this state we successfully connected to OpenRGB server, starting recv thread

    if (pthread_create(&openrgb_recv_thread_id, NULL, openrgb_recv_thread, NULL) != 0) {
        perror("Failed to create receive thread");
        close(openrgb_socket);
        return;
    }

    // now all responses by openrgb will be received by recv thread.
    // requesting version.
    openrgb_request_protocol_version();
    logger("Requested protocol version, waiting...\n");
    uint8_t timeout_counter = 0;
    while (openrgb_using_version == -1) {
        logger("Waiting... current openrgb version: %d", openrgb_using_version);
        usleep(100000);
        timeout_counter++;
        if (timeout_counter > 10) {
            // if no protocol version received within 1s, assuming that server does not
            // supports protocol versioning and working at version 0
            openrgb_using_version = 0;
            break;
        }
    }
    logger("OpenRGB negotiated version is %d!\n", openrgb_using_version);

    logger("Setting client's name\n");
    openrgb_set_client_name();
}

void openrgb_request_protocol_version() {
    logger("Requesting protocol version.");
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
    name[7] = openrgb_using_version + '0';
    uint8_t *header = NULL;
    openrgb_init_header(&header, 0, OPENRGB_NET_PACKET_ID_SET_CLIENT_NAME, strlen("piled vx"));
    pthread_mutex_lock(&openrgb_send_mutex);
    send(openrgb_socket, header, 16, 0);
    send(openrgb_socket, name, strlen("piled vx"), 0);
    pthread_mutex_unlock(&openrgb_send_mutex);
    free(header);
    free(name);
}

uint32_t request_controller_count() {
    // // stopping listening for responses
    // suspend_server = 1;
    // pthread_join(recv_thread_id, NULL);
    // printf("Requesting controller count");
    // uint8_t *header = generate_packet(0, OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT, 0, NULL);
    // if (send(openrgb_socket, header, 20, 0) < 0) {
    //     perror("Failed to send data");
    //     close(openrgb_socket);
    //     free(header);
    // }
    // uint8_t response[16];
    // int recv_size = recv(openrgb_socket, response, sizeof(response), 0);
    // printf("Received %d bytes from OpenRGB:\n", recv_size);
    // for (int i = 0; i < recv_size; i++) {
    //     printf("%x ", response[i]);
    //     if (response[i] != header[i] && i < 12) {
    //         printf("Received unwanted package! Aborting.");
    //         suspend_server = 0;
    //         pthread_create(&recv_thread_id, NULL, recv_thread, NULL);
    //         return -1;
    //     }
    // }
    // printf("\n");
    // recv_size = recv(openrgb_socket, response, sizeof(response), 0);
    // printf("Received %d bytes with device count from OpenRGB:\n", recv_size);
    // for (int i = 0; i < recv_size; i++) {
    //     printf("%x ", response[i]);
    // }
    // uint32_t device_count = 0;
    // memcpy(&device_count, response, 4);
    // printf("\nGot device count: %d\n", device_count);
    // suspend_server = 0;
    // pthread_create(&recv_thread_id, NULL, openrgb_recv_thread, NULL);
    // return device_count;
}

void *openrgb_recv_thread(void *arg) {
    logger("Started OpenRGB receive thread!\n");
    uint8_t response[1024];
    int recv_size;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    if (setsockopt(openrgb_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Failed to set socket timeout");
    }

    while (!stop_server) {
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

        logger("OpenRGB Parser: Starting parsing buffer with size %d", recv_size);
        printf("Buffer:\n");
        for (int i = 0; i < recv_size; i++) {
            printf("%x ", response[i]);
        }
        printf("\n");

        // сhecking for magick
        if (response[0] != 'O' || response[1] != 'R' || response[2] != 'G' || response[3] != 'B') {
            printf("Magick wrong! Not a OpenRGB package!\n");
            return NULL;
        }

        uint32_t pkt_dev_idx = 0;
        memcpy(&pkt_dev_idx, response + 4, 4);
        printf("Got device index: %d\n", pkt_dev_idx);

        uint32_t pkt_id = 0;
        memcpy(&pkt_id, response + 8, 4);
        printf("Got packet id: %d\n", pkt_id);

        uint32_t pkt_size = 0;
        memcpy(&pkt_size, response + 12, 4);
        printf("Got packet size: %d\n", pkt_size);
        // entire header received, now receive the data

        switch (pkt_id) {
        case OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_DATA: {
            printf("NET_PACKET_ID_REQUEST_CONTROLLER_DATA not implemented.\n");

            break;
        }
        case OPENRGB_NET_PACKET_ID_REQUEST_PROTOCOL_VERSION: {
            printf("NET_PACKET_ID_REQUEST_PROTOCOL_VERSION\n");
            uint8_t *version_data;
            printf("Receiving data packet...\n");

            uint32_t bytes_received = 0;
            uint32_t tmp_bytes_received;

            while (bytes_received < pkt_size) {
                tmp_bytes_received = recv(openrgb_socket, response + bytes_received, pkt_size - bytes_received, 0);
                if (tmp_bytes_received <= 0) {
                    printf("Error: Failed to receive data. Received %d bytes so far.\n", bytes_received);
                    return NULL;
                }
                bytes_received += tmp_bytes_received;
            }
            printf("Received %d bytes.\n", bytes_received);
            uint32_t openrgb_version = 0;
            memcpy(&openrgb_version, response, 4);
            openrgb_using_version =
                openrgb_version <= OPENRGB_SUPPORTED_VERSION ? openrgb_version : OPENRGB_SUPPORTED_VERSION;
            printf("\nOpenRGB Server's Version: %d, Client max supported version: %d, Using version: %d\n",
                   openrgb_version, OPENRGB_SUPPORTED_VERSION, openrgb_using_version);

            break;
        }
        }
    }

    return NULL;
}

struct openrgb_controller_data request_controller_data(uint32_t pkt_dev_idx) {
    // requests controller data and parses result
    // why we negotiated version,
    // in OpenRGB SDK Server tab there is printed "4" protocol version, but responses are as for 0 version?
    // suspend_server = 1;
    // pthread_join(recv_thread_id, NULL);
    // printf("Requesting controller #%d data\n", pkt_dev_idx);
    // uint8_t *header = generate_packet(pkt_dev_idx, OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_DATA, 0, NULL);
    // if (send(openrgb_socket, header, 20, 0) < 0) {
    //     perror("Failed to send data");
    //     close(openrgb_socket);
    //     free(header);
    // }
    // uint8_t response[20];
    // int recv_size = recv(openrgb_socket, response, sizeof(response), 0);
    // printf("Received %d bytes from OpenRGB:\n", recv_size);
    // for (int i = 0; i < recv_size; i++) {
    //     printf("%x ", response[i]);
    //     if (response[i] != header[i] && i < 12) {
    //         printf("Received unwanted package! Aborting.");
    //         suspend_server = 0;
    //         pthread_create(&recv_thread_id, NULL, recv_thread, NULL);
    //         // return -1;
    //     }
    // }
    // printf("\n");
    // uint32_t response_data_size = 0;
    // memcpy(&response_data_size, &response[12], 4);
    // uint8_t response_data[response_data_size];
    // recv_size = recv(openrgb_socket, response_data, response_data_size, 0);
    // printf("Received %d bytes with device data from OpenRGB. Response data size: %d\n:\n", recv_size,
    //        response_data_size);
    // for (int i = 0; i < recv_size; i++) {
    //     printf("%x ", response_data[i]);
    // }
    // printf("\nStarting parsing.........\n");

    // struct openrgb_controller_data result;
    // uint32_t offset = 0;
    // memcpy(&result.data_size, response_data, 4);
    // offset += 4;
    // printf("Data size: %d\n", result.data_size);
    // memcpy(&result.type, response_data + offset, 4);
    // offset += 4;
    // printf("Type: %d\n", result.type);
    // memcpy(&result.name_len, response_data + offset, 2);
    // offset += 2;
    // printf("Name length: %d\n", result.name_len);
    // result.name = malloc(result.name_len + 1);
    // memcpy(result.name, response_data + offset, result.name_len);
    // offset += result.name_len;
    // printf("Parsed name by bytes:\n");
    // for (int i = 0; i < result.name_len; i++) {
    //     printf("%x ", result.name[i]);
    // }
    // printf("\nName: %s\n", result.name);

    // memcpy(&result.vendor_len, response_data + offset, 2);
    // offset += 2;
    // result.vendor = malloc(result.vendor_len);
    // memcpy(result.vendor, response_data + offset, result.vendor_len);
    // offset += result.vendor_len;
    // printf("Vendor: %s\n", result.vendor);

    // memcpy(&result.description_len, response_data + offset, 2);
    // offset += 2;
    // result.description = malloc(result.description_len);
    // memcpy(result.description, response_data + offset, result.description_len);
    // offset += result.description_len;
    // printf("Description: %s\n", result.description);

    // memcpy(&result.version_len, response_data + offset, 2);
    // offset += 2;
    // result.version = malloc(result.version_len);
    // memcpy(result.version, response_data + offset, result.version_len);
    // offset += result.version_len;
    // printf("Version: %s\n", result.version);

    // memcpy(&result.serial_len, response_data + offset, 2);
    // offset += 2;
    // result.serial = malloc(result.serial_len);
    // memcpy(result.serial, response_data + offset, result.serial_len);
    // offset += result.serial_len;
    // printf("Serial: %s\n", result.serial);

    // //  why location skipped in actual packets?
    // //  memcpy(&result.location_len, response_data + offset, 2);
    // //  offset += 2;
    // //  result.location = malloc(result.location_len);
    // //  memcpy(result.location, response_data + offset, result.location_len);
    // //  offset += result.location_len;
    // //  printf("Location size: %d, Location: %s\n", result.location_len, result.location);

    // memcpy(&result.num_modes, response_data + offset, 2);
    // offset += 2;
    // memcpy(&result.active_mode, response_data + offset, 4);
    // offset += 4;
    // printf("Modes count: %d, active mode: %d\n", result.num_modes, result.active_mode);

    // // parsing modes..
    // struct openrgb_mode_data *mode_data = malloc(sizeof(struct openrgb_mode_data) * result.num_modes);
    // for (int mode = 0; mode < result.num_modes; mode++) {
    //     printf("Parsing mode #%d of %d\n", mode + 1, result.num_modes);
    //     memcpy(&mode_data[mode].mode_name_len, response_data + offset, 2);
    //     offset += 2;
    //     mode_data[mode].mode_name = malloc(mode_data[mode].mode_name_len);
    //     memcpy(mode_data[mode].mode_name, response_data + offset, mode_data[mode].mode_name_len);
    //     offset += mode_data[mode].mode_name_len;
    //     printf("Mode name: %s\n", mode_data[mode].mode_name);

    //     memcpy(&mode_data[mode].mode_value, response_data + offset, 4);
    //     offset += 4;
    //     memcpy(&mode_data[mode].mode_flags, response_data + offset, 4);
    //     offset += 4;
    //     memcpy(&mode_data[mode].mode_speed_min, response_data + offset, 4);
    //     offset += 4;
    //     memcpy(&mode_data[mode].mode_speed_max, response_data + offset, 4);
    //     offset += 4;
    //     // if (OPENRGB_SUPPORTED_VERSION >= 3) {
    //     //     memcpy(&mode_data[mode].mode_brightness_min, response_data + offset, 4);
    //     //     offset += 4;
    //     //     memcpy(&mode_data[mode].mode_brightness_max, response_data + offset, 4);
    //     //     offset += 4;
    //     // }
    //     memcpy(&mode_data[mode].mode_colors_min, response_data + offset, 4);
    //     offset += 4;
    //     memcpy(&mode_data[mode].mode_colors_max, response_data + offset, 4);
    //     offset += 4;
    //     memcpy(&mode_data[mode].mode_speed, response_data + offset, 4);
    //     offset += 4;
    //     // if (OPENRGB_SUPPORTED_VERSION >= 3) {
    //     //     memcpy(&mode_data[mode].mode_brightness, response_data + offset, 4);
    //     //     offset += 4;
    //     // }
    //     memcpy(&mode_data[mode].mode_direction, response_data + offset, 4);
    //     offset += 4;
    //     memcpy(&mode_data[mode].mode_color_mode, response_data + offset, 4);
    //     offset += 4;
    //     memcpy(&mode_data[mode].mode_num_colors, response_data + offset, 2);
    //     offset += 2;
    //     printf("Number of colors: %d\n", mode_data[mode].mode_num_colors);
    //     mode_data[mode].mode_colors = malloc(4 * mode_data[mode].mode_num_colors);
    //     memcpy(&mode_data[mode].mode_colors, response_data + offset, 4 * mode_data[mode].mode_num_colors);
    //     offset += 4 * mode_data[mode].mode_num_colors;
    // }
    // // parsing zones..........
    // memcpy(&result.num_zones, response_data + offset, 2);
    // offset += 2;
    // struct openrgb_zone_data *zone_data = malloc(sizeof(struct openrgb_zone_data) * result.num_zones);
    // for (int zone = 0; zone < result.num_zones; zone++) {
    //     printf("Parsing zone #%d of %d\n", zone, result.num_zones);
    //     memcpy(&zone_data[zone].zone_name_len, response_data + offset, 2);
    //     offset += 2;
    //     zone_data[zone].zone_name = malloc(zone_data[zone].zone_name_len);
    //     memcpy(zone_data[zone].zone_name, response_data + offset, zone_data[zone].zone_name_len);
    //     offset += zone_data[zone].zone_name_len;
    //     printf("Zone name: %s\n", zone_data[zone].zone_name);

    //     memcpy(&zone_data[zone].zone_type, response_data + offset, 4);
    //     offset += 4;

    //     memcpy(&zone_data[zone].zone_leds_min, response_data + offset, 4);
    //     offset += 4;
    //     memcpy(&zone_data[zone].zone_leds_max, response_data + offset, 4);
    //     offset += 4;

    //     memcpy(&zone_data[zone].zone_leds_count, response_data + offset, 4);
    //     offset += 4;

    //     memcpy(&zone_data[zone].zone_matrix_len, response_data + offset, 2);
    //     offset += 2;

    //     if (zone_data[zone].zone_matrix_len > 0) {
    //         printf("Zone matrix parsing not supported, skipping!\n");
    //         // need to calculate offset...
    //         // memcpy(&zone_data[zone].zone_matrix_height, response_data + offset, 4);
    //         offset += 4;
    //         // memcpy(&zone_data[zone].zone_matrix_width, response_data + offset, 4);
    //         offset += 4;

    //         // there could be zone_matrix_data...
    //         offset += zone_data[zone].zone_matrix_len - 8;
    //     }
    // }

    // // parsing leds
    // memcpy(&result.num_leds, response_data + offset, 2);
    // offset += 2;
    // struct openrgb_led_data *led_data = malloc(sizeof(struct openrgb_led_data) * result.num_leds);
    // for (int led = 0; led < result.num_leds; led++) {
    //     printf("Parsing led #%d of %d\n", led, result.num_leds);
    //     memcpy(&led_data[led].led_name_len, response_data + offset, 2);
    //     offset += 2;
    //     led_data[led].led_name = malloc(led_data[led].led_name_len);
    //     memcpy(led_data[led].led_name, response_data + offset, led_data[led].led_name_len);
    //     offset += led_data[led].led_name_len;
    //     memcpy(&led_data[led].led_value, response_data + offset, 4);
    //     offset += 4;
    // }

    // memcpy(&result.num_colors, response_data + offset, 2);
    // offset += 2;
    // for (int color = 0; color < result.num_colors; color++) {
    //     printf("Parsing color #%d of %d\n", color, result.num_colors);
    //     uint32_t parsed_color = 0;
    //     memcpy(&parsed_color, response_data + offset, 4);
    //     printf("Parsed color: %x\n", parsed_color);
    // }

    // pthread_create(&recv_thread_id, NULL, recv_thread, NULL);
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
