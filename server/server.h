#ifndef SERVER_H
#define SERVER_H

#include "../utils/utils.h"
#include <signal.h>

extern volatile sig_atomic_t stop_server, is_suspended;

void add_client_fd(int client_fd);
void remove_client_fd(int client_fd);
void stop_animation();
void *handle_client(void *client_sock);
int start_server(int pi, int port);
void send_info_about_new_color(struct Color color);

#endif // SERVER_H
