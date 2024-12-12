#include "globals/globals.h"
#include "parser/config.h"
#include "pigpiod_if2.h"
#include "rgb/gpio.h"
#include "rgb/openrgb.h"
#include "server/server.h"
#include "utils/utils.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef libwebsockets_FOUND
#include "server/ws.h"
#endif

#ifdef microhttpd_FOUND
#include "server/http.h"
#endif

void handle_sigint(int sig) {
    logger(MAIN, "Stopping server!");
    stop_server = 1;
    openrgb_stop_server = 1;
    openrgb_exit = 1;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

    // ingoring SIGPIPE, which may be called when client sent package but ignores output
    signal(SIGPIPE, SIG_IGN);

    if (load_config() != 0) {
        return -1;
    }

    parse_args(argc, argv);

    if (OPENRGB_SERVER) {
        logger(MAIN, "OpenRGB server IP is set, starting OpenRGB!");
        pthread_t orgb_thread;
        pthread_create(&orgb_thread, NULL, openrgb_init, NULL);
    } else {
        logger(MAIN, "Not starting OpenRGB since OpenRGB server IP not set.");
    }

    pi = pigpio_start(PI_ADDR, PI_PORT);
    if (pi < 0) {
        logger(MAIN, "Pigpio initialization failed.\n");
        return 1;
    }
    logger(MAIN, "Connected to pigpio daemon successfully!");

    set_mode(pi, RED_PIN, PI_OUTPUT);
    set_mode(pi, GREEN_PIN, PI_OUTPUT);
    set_mode(pi, BLUE_PIN, PI_OUTPUT);

    set_color(pi, (struct Color){0, 0, 0});

#ifdef libwebsockets_FOUND
    ws_server_init(pi);
#endif

#ifdef microhttpd_FOUND
    start_http_server(pi);
#endif

    if (start_server(pi, 3384) < 0) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }

    logger(MAIN, "See you next time!");
    pigpio_stop(pi);
    openrgb_shutdown();
    free(PI_ADDR);
    free(PI_PORT);
    free(SHARED_SECRET);
    free(OPENRGB_SERVER);
    return 0;
}
