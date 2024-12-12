#include "server.h"
#include "../globals/globals.h"
#include "../parser/parser.h"
#include "../pigpio/pigpiod_if2.h"
#include "../rgb/gpio.h"
#include "../utils/utils.h"
#include <arpa/inet.h>
#include <openssl/hmac.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t stop_server = 0, is_suspended = 0;
int *clients_fds = NULL;
int clients_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void add_client_fd(int client_fd) {
    pthread_mutex_lock(&clients_mutex);

    int *new_clients_fds = realloc(clients_fds, sizeof(int) * (clients_count + 1));
    if (new_clients_fds == NULL) {
        logger(TCP, "Failed to realloc memory for clients_fds");
        close(client_fd);
    } else {
        clients_fds = new_clients_fds;
        clients_fds[clients_count++] = client_fd;
    }
    logger(TCP, "Client with fd %d connected\n", client_fd);
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client_fd(int client_fd) {
    pthread_mutex_lock(&clients_mutex);

    int index = -1;
    for (int i = 0; i < clients_count; i++) {
        if (clients_fds[i] == client_fd) {
            index = i;
            break;
        }
    }

    if (index != -1) {
        for (int i = index; i < clients_count - 1; i++) {
            clients_fds[i] = clients_fds[i + 1];
        }
        clients_count--;

        int *new_clients_fds = realloc(clients_fds, sizeof(int) * clients_count);
        if (new_clients_fds || clients_count == 0) { // realloc could return NULL if size is 0
            clients_fds = new_clients_fds;
        }
    }
    logger(TCP, "Client with fd %d disconnected\n", client_fd);
    pthread_mutex_unlock(&clients_mutex);
}

void stop_animation() {
    logger_debug(TCP, "Stop animation function called.");
    // pthread_mutex_lock(&animation_mutex);
    if (is_animating) {
        is_animating = 0;
        if (animation_thread) {
            pthread_join(animation_thread, NULL);
            animation_thread = 0;
        }
    }
    // pthread_mutex_unlock(&animation_mutex);
}

void *handle_client(void *client_sock) {
    int client_fd = *(int *)client_sock;
    free(client_sock);

    add_client_fd(client_fd);

    unsigned char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    fd_set read_fds;
    struct timeval timeout;

    while (!stop_server) {
        // Set up the fd_set and timeout before each select call
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);

        timeout.tv_sec = 1; // Timeout after 1 second of no activity
        timeout.tv_usec = 0;

        int activity = select(client_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select");
            break;
        } else if (activity == 0) {
            // Timeout occurred, no data within timeout period
            continue;
        }

        // Check if there’s data to read
        if (FD_ISSET(client_fd, &read_fds)) {
            int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes_received < 0) {
                perror("recv");
                break;
            } else if (bytes_received == 0) {
                break;
            }

            logger_debug(TCP, "Received: %d bytes.", bytes_received);
            struct parse_result result = parse_message(buffer);

            if (is_suspended && result.OP != SYS_TOGGLE_SUSPEND) {
                logger(TCP, "Received package, but PiLED is in *suspended* mode! Ignoring.");
                continue;
            }

            logger_debug(TCP, "Result of parsing: %d", result.result);
            if (result.result == 0) {
                logger_debug(TCP, "Successfully parsed and checked packet, processing. v%d", result.version);
                switch (result.version) {
                case 4:
                case 3: {
                    logger_debug(TCP, "v%d, OP is: %d", result.version, result.OP);
                    switch (result.OP) {
                    case LED_SET_COLOR: {
                        logger(TCP, "Requested LED_SET_COLOR with %d %d %d on %d seconds, setting.", result.RED,
                               result.GREEN, result.BLUE, result.duration);
                        set_color_duration(pi, (struct Color){result.RED, result.GREEN, result.BLUE}, result.duration);
                        break;
                    }
                    case LED_GET_CURRENT_COLOR: {
                        logger(TCP, "Requested LED_GET_CURRENT_COLOR, sending...");
                        send_info_about_color();
                        break;
                    }
                    case ANIM_SET_FADE: {
                        logger(TCP, "Requested ANIM_SET_FADE.");
                        stop_animation();
                        struct fade_animation_args *args = malloc(sizeof(struct fade_animation_args));
                        args->pi = pi;
                        args->speed = result.speed;
                        pthread_mutex_lock(&animation_mutex);
                        if (pthread_create(&animation_thread, NULL, start_fade_animation, (void *)args) != 0) {
                            perror("Failed to create thread");
                            free(args);
                        } else {
                            logger(TCP, "Started fade animation thread!");
                        }
                        pthread_mutex_unlock(&animation_mutex);
                        break;
                    }
                    case ANIM_SET_PULSE: {
                        logger(TCP, "Requested ANIM_SET_PULSE");
                        stop_animation();
                        struct pulse_animation_args *args = malloc(sizeof(struct pulse_animation_args));
                        args->pi = pi;
                        args->color = (struct Color){result.RED, result.GREEN, result.BLUE};
                        args->duration = result.duration;
                        pthread_mutex_lock(&animation_mutex);
                        if (pthread_create(&animation_thread, NULL, start_pulse_animation, (void *)args) != 0) {
                            perror("Failed to create thread");
                            free(args);
                        } else {
                            logger(TCP, "Started pulse animation thread!");
                        }
                        pthread_mutex_unlock(&animation_mutex);
                        break;
                    }
                    case SYS_TOGGLE_SUSPEND: {
                        logger(TCP, "Requested SYS_TOGGLE_SUSPEND.");
                        stop_animation();
                        is_suspended = !is_suspended;
                        set_color_duration(pi,
                                           (struct Color){is_suspended ? 0 : result.RED,
                                                          is_suspended ? 0 : result.GREEN,
                                                          is_suspended ? 0 : result.BLUE},
                                           result.duration);
                        break;
                    }
                    }
                    break;
                }
                case 2: {
                    logger(TCP, "v2, setting with duration");
                    set_color_duration(pi, (struct Color){result.RED, result.GREEN, result.BLUE}, result.duration);
                    break;
                }
                case 1:
                default: {
                    logger(TCP, "v1, setting without duration");
                    set_color_duration(pi, (struct Color){result.RED, result.GREEN, result.BLUE}, 0);
                    break;
                }
                }
            }
        }
    }

    close(client_fd);
    remove_client_fd(client_fd);
    return NULL;
}

int start_server(int pi, int port) {
    int server_fd;
    struct sockaddr_in server_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(server_fd);
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    logger(TCP, "Server listening on port %d", port);

    fd_set read_fds;
    struct timeval timeout;

    while (!stop_server) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0 && !stop_server) {
            perror("select");
            break;
        }

        if (activity > 0 && FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            int *client_fd = malloc(sizeof(int));
            if (!client_fd) {
                perror("malloc");
                continue;
            }

            *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
            if (*client_fd < 0) {
                perror("accept");
                free(client_fd);
                continue;
            }

            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client, client_fd) != 0) {
                perror("pthread_create");
                close(*client_fd);
                free(client_fd);
            } else {
                pthread_detach(thread_id);
            }
        }
    }

    // Close the server socket
    close(server_fd);
    return 0;
}

void send_info_about_color() {
    struct Color color = {get_PWM_dutycycle(pi, RED_PIN), get_PWM_dutycycle(pi, GREEN_PIN),
                          get_PWM_dutycycle(pi, BLUE_PIN)};
    logger_debug(TCP, "Sending info about current color: %d %d %d", color.RED, color.GREEN, color.BLUE);
    // generating HEADER
    uint8_t HEADER[18];

    uint64_t current_time = time(NULL);
    memcpy(HEADER, &current_time, 8);

    // generating NONCE
    srand((unsigned int)time(NULL));
    uint64_t rand_num = ((uint64_t)rand() << 32) | rand();
    memcpy(HEADER + 8, &rand_num, 8);

    uint8_t version = 4;
    uint8_t OP = SYS_COLOR_CHANGED;
    HEADER[16] = version;
    HEADER[17] = OP;

    // generating PAYLOAD
    uint8_t PAYLOAD[5];
    PAYLOAD[0] = color.RED;
    PAYLOAD[1] = color.GREEN;
    PAYLOAD[2] = color.BLUE;
    PAYLOAD[3] = 0; // duration
    PAYLOAD[4] = 0; // steps

    // combine HEADER and PAYLOAD
    uint8_t header_with_payload[23];
    memcpy(header_with_payload, HEADER, 18);
    memcpy(header_with_payload + 18, PAYLOAD, 5);

    // generating new hmac
    char *key = SHARED_SECRET;
    int key_length = strlen(key);
    unsigned char GENERATED_HMAC[32];
    unsigned int hmac_len;
    HMAC(EVP_sha256(), key, key_length, header_with_payload, 23, GENERATED_HMAC, &hmac_len);
    uint8_t tcp_package[55];
    memcpy(tcp_package, HEADER, 18);
    memcpy(tcp_package + 18, GENERATED_HMAC, 32);
    memcpy(tcp_package + 50, PAYLOAD, 5);
    for (int client = 0; client < clients_count; client++) {
        logger_debug(TCP, "Sending package to client fd %d", clients_fds[client]);
        ssize_t sent_bytes = send(clients_fds[client], tcp_package, 55, MSG_DONTWAIT);
        if (sent_bytes < 0) {
            perror("send");
            logger_debug(TCP, "Failed to send to client fd %d, removing client", clients_fds[client]);
            close(clients_fds[client]);
            remove_client_fd(clients_fds[client]);
        }
    }
}