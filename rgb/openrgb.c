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
    if (send(openrgb_socket, header, 20, 0) < 0) {
        perror("Failed to send data");
        close(openrgb_socket);
        free(header);
        return;
    }

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