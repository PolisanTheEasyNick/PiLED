#include "globals.h"
#include "parser.h"
#include "pigpiod_if2.h"
#include "server.h"
#include "utils.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void handle_sigint(int sig) {
    logger("Stopping server!");
    stop_server = 1;
}

int main() {
    signal(SIGINT, handle_sigint);
    parse_config("../config.conf");

    int pi = pigpio_start(PI_ADDR, PI_PORT);
    if (pi < 0) {
        fprintf(stderr, "Pigpio initialization failed.\n");
        return 1;
    }
    logger("Connected to pigpio daemon successfully!");

    set_mode(pi, RED_PIN, PI_OUTPUT);
    set_mode(pi, GREEN_PIN, PI_OUTPUT);
    set_mode(pi, BLUE_PIN, PI_OUTPUT);

    if (start_server(pi, 3384) < 0) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }
    logger("See you next time!");
    pigpio_stop(pi);
    free(PI_ADDR);
    free(PI_PORT);
    free(SHARED_SECRET);
    return 0;
}
