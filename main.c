#include "gpio.h"
#include "pigpiod_if2.h"
#include "server.h"
#include "utils.h"
#include <stdio.h>

int main() {
    int pi = pigpio_start(NULL, NULL);
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

    pigpio_stop(pi);
    return 0;
}
