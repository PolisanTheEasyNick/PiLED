#ifndef SERVER_H
#define SERVER_H

#include <signal.h>

extern volatile sig_atomic_t stop_server;

int start_server(int pi, int port);

#endif // SERVER_H
