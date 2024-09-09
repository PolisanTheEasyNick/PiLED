#include "globals/globals.h"
#include "parser/parser.h"
#include "pigpiod_if2.h"
#include "rgb/gpio.h"
#include "rgb/openrgb.h"
#include "server/server.h"
#include "utils/utils.h"
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_sigint(int sig) {
    logger("Stopping server!");
    stop_server = 1;
}

char config_file[256];

void parse_args(int argc, char *argv[]) {
    int opt;
    static struct option long_options[] = {{"server", required_argument, 0, 's'},
                                           {"port", required_argument, 0, 'p'},
                                           {"RED", required_argument, 0, 'R'},
                                           {"GREEN", required_argument, 0, 'G'},
                                           {"BLUE", required_argument, 0, 'B'},
                                           {"SHARED_SECRET", required_argument, 0, 'S'},
                                           {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "c:s:p:R:G:B:S:", long_options, NULL)) != -1) {
        switch (opt) {
        case 's':
            snprintf(PI_ADDR, sizeof(PI_ADDR), "%s", optarg);
            logger("RPi Server address set to: %s", PI_ADDR);
            break;
        case 'p':
            snprintf(PI_PORT, sizeof(PI_PORT), "%s", optarg);
            logger("Server port set to: %d", PI_PORT);
            break;
        case 'R':
            RED_PIN = atoi(optarg);
            logger("Red pin set to: %d", RED_PIN);
            break;
        case 'G':
            GREEN_PIN = atoi(optarg);
            logger("Green pin set to: %d", GREEN_PIN);
            break;
        case 'B':
            BLUE_PIN = atoi(optarg);
            logger("Blue pin set to: %d", BLUE_PIN);
            break;
        case 'S':
            snprintf(SHARED_SECRET, sizeof(SHARED_SECRET), "%s", optarg);
            logger("Shared secret set to: %s", SHARED_SECRET);
            break;
        default:
            logger("Unknown option or missing argument. Exiting.");
            exit(EXIT_FAILURE);
        }
    }
}

uint8_t try_load_config(const char *config_path) {
    logger("Trying to load config from: %s", config_path);
    return parse_config(config_path);
}

int load_config() {
    char *home_path = getenv("HOME");

    if (home_path) {
        snprintf(config_file, sizeof(config_file), "%s%s", home_path, "/.config/piled.conf");
        if (try_load_config(config_file) == 0) {
            return 0;
        }
    }

    if (try_load_config("../piled.conf") == 0) {
        return 0;
    }

    if (try_load_config("/etc/config/piled.conf") != 0) {
        logger("Can't load any of the configs! Aborting.");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    openrgb_connect();
    // printf("Got controllers: %d", controllers);
    return 0;
    if (load_config() != 0) {
        return -1;
    }

    parse_args(argc, argv);

    int pi = pigpio_start(PI_ADDR, PI_PORT);
    if (pi < 0) {
        fprintf(stderr, "Pigpio initialization failed.\n");
        return 1;
    }
    logger("Connected to pigpio daemon successfully!");

    set_mode(pi, RED_PIN, PI_OUTPUT);
    set_mode(pi, GREEN_PIN, PI_OUTPUT);
    set_mode(pi, BLUE_PIN, PI_OUTPUT);

    set_color(pi, (struct Color){0, 0, 0});

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
