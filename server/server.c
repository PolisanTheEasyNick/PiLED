#include "server.h"
#include "../globals/globals.h"
#include "../parser/parser.h"
#include "../pigpio/pigpiod_if2.h"
#include "../rgb/gpio.h"
#include "../utils/utils.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t stop_server = 0;

int start_server(int pi, int port) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    unsigned char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    struct timeval timeout;
    fd_set read_fds;
    pthread_t thread_id;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
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

    logger("Server listening on port %d\n", port);

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
            client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_fd < 0) {
                perror("accept");
                continue;
            }

            int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes_received < 0) {
                perror("recv");
            } else {
                logger("Received: %d bytes.\n", bytes_received);
                struct parse_result result = parse_message(buffer);
                logger("Result of parsing: %d", result.result);
                if (result.result == 0) {
                    logger("Successfully parsed and checked packet, processing. v%d", result.version);
                    switch (result.version) {
                    case 4:
                    case 3: {
                        logger("v3, OP is: %d", result.OP);
                        switch (result.OP) {
                        case LED_SET_COLOR: { // SET COLOR
                            set_color_duration(pi, (struct Color){result.RED, result.GREEN, result.BLUE},
                                               result.duration);
                            break;
                        }
                        case LED_GET_CURRENT_COLOR: {        // GET COLOR
                            unsigned char current_color[11]; // timestamp (8 bytes) + 3 bytes of colors RGB
                            uint64_t current_time = time(NULL);
                            struct Color last_color = {get_PWM_dutycycle(pi, RED_PIN), get_PWM_dutycycle(pi, GREEN_PIN),
                                                       get_PWM_dutycycle(pi, BLUE_PIN)};
                            logger("Current color is: 0x%x 0x%x 0x%x", last_color.RED, last_color.GREEN,
                                   last_color.BLUE);
                            memset(current_color, 0, 11);
                            memcpy(current_color, (unsigned char *)&current_time, 8);
                            current_color[8] = last_color.RED;
                            current_color[9] = last_color.GREEN;
                            current_color[10] = last_color.BLUE;
                            logger("Parsed color, sending this package:");
#ifdef DEBUG
                            for (short i = 0; i < 11; i++) {
                                printf("%x ", current_color[i]);
                            }
                            printf("\n");
#endif
                            send(client_fd, current_color, 11, 0);
                            break;
                        }
                        case ANIM_SET_FADE: {
                            pthread_mutex_lock(&animation_mutex);
                            if (is_animating) {
                                is_animating = 0;
                                pthread_join(animation_thread, NULL);

                                pthread_mutex_unlock(&animation_mutex);
                            } else {
                                pthread_mutex_unlock(&animation_mutex);
                            }

                            struct fade_animation_args *args = malloc(sizeof(struct fade_animation_args));
                            args->pi = pi;
                            args->speed = result.speed;

                            if (pthread_create(&animation_thread, NULL, start_fade_animation, (void *)args) != 0) {
                                perror("Failed to create thread");
                                free(args);
                            } else {
                                logger("Started fade animation thread!");
                            }
                            break;
                        }
                        case ANIM_SET_PULSE: {
                            pthread_mutex_lock(&animation_mutex);
                            if (is_animating) {
                                is_animating = 0;
                                pthread_join(animation_thread, NULL);
                                pthread_mutex_unlock(&animation_mutex);
                            } else {
                                pthread_mutex_unlock(&animation_mutex);
                            }

                            struct pulse_animation_args *args = malloc(sizeof(struct pulse_animation_args));
                            args->pi = pi;
                            args->color = (struct Color){result.RED, result.GREEN, result.BLUE};
                            args->duration = result.duration;

                            if (pthread_create(&animation_thread, NULL, start_pulse_animation, (void *)args) != 0) {
                                perror("Failed to create thread");
                                free(args);
                            } else {
                                logger("Started pulse animation thread!");
                            }
                            break;
                        }
                        }
                        break;
                    }
                    case 2: {
                        logger("v2, setting with duration");
                        set_color_duration(pi, (struct Color){result.RED, result.GREEN, result.BLUE}, result.duration);
                        break;
                    }
                    case 1:
                    default: {
                        logger("v1, setting without duration");
                        set_color_duration(pi, (struct Color){result.RED, result.GREEN, result.BLUE}, 0);
                        break;
                    }
                    }
                }
            }

            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}
