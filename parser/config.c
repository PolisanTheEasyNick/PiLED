#include "config.h"
#include "../globals/globals.h"
#include "../utils/utils.h"
#include <getopt.h>
#include <libconfig.h>
#include <stdlib.h>
#include <string.h>

uint8_t parse_config(const char *config_file) {
    config_t cfg;
    config_init(&cfg);

    if (!config_read_file(&cfg, config_file)) {
        fprintf(stderr, "Error reading config file: %s\n", config_error_text(&cfg));
        config_destroy(&cfg);
        return 1;
    }
    logger("Opened config file");
#ifndef ORGBCONFIGURATOR
    const char *addr;
    if (!config_lookup_string(&cfg, "PI_ADDR", &addr)) {
        fprintf(stderr, "Missing PI_ADDR in config file, using default (NULL)\n");
        PI_ADDR = NULL;
    } else {
        PI_ADDR = malloc(strlen(addr) + 1);
        strncpy(PI_ADDR, addr, strlen(addr));
        PI_ADDR[strlen(addr)] = 0;
    }

    const char *port;
    if (!config_lookup_string(&cfg, "PI_PORT", &port)) {
        fprintf(stderr, "Missing PI_PORT in config file, using default (8888)\n");
        PI_PORT = NULL;
    } else {
        PI_PORT = malloc(strlen(port) + 1);
        strncpy(PI_PORT, port, strlen(port));
        PI_PORT[strlen(port)] = 0;
    }

    if (!config_lookup_int(&cfg, "RED_PIN", &RED_PIN)) {
        fprintf(stderr, "Missing RED_PIN in config file!\n");
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }

    if (!config_lookup_int(&cfg, "GREEN_PIN", &GREEN_PIN)) {
        fprintf(stderr, "Missing GREEN_PIN in config file!\n");
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }
    if (!config_lookup_int(&cfg, "BLUE_PIN", &BLUE_PIN)) {
        fprintf(stderr, "Missing BLUE_PIN in config file!\n");
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }
    const char *secret;
    if (!config_lookup_string(&cfg, "SHARED_SECRET", &secret)) {
        fprintf(stderr, "Missing SHARED_SECRET in config file\n");
        SHARED_SECRET = NULL;
        return -1;
    }
    SHARED_SECRET = malloc(strlen(secret) + 1);
    strncpy(SHARED_SECRET, secret, strlen(secret));
    SHARED_SECRET[strlen(secret)] = 0;
#endif
    const char *openrgb_addr;
    if (!config_lookup_string(&cfg, "OPENRGB_SERVER", &openrgb_addr)) {
        fprintf(stderr, "Missing OPENRGB_SERVER in config file\n");
        OPENRGB_SERVER = NULL;
    } else {
        OPENRGB_SERVER = malloc(strlen(openrgb_addr) + 1);
        strncpy(OPENRGB_SERVER, openrgb_addr, strlen(openrgb_addr));
        OPENRGB_SERVER[strlen(openrgb_addr)] = 0;
    }

    if (!config_lookup_int(&cfg, "OPENRGB_PORT", &OPENRGB_PORT)) {
        fprintf(stderr, "Missing OPENRGB_PORT in config file, using default 6742\n");
        OPENRGB_PORT = 6742;
    }
#ifndef ORGBCONFIGURATOR
    logger("Passed config:\nRaspberry Pi address: %s\nPort: %s\nRed pin: %d\nGreen pin: %d\nBlue pin: %d\nShared "
           "secret: %s\nOpenRGB server: %s\nOpenRGB Port: %d\n",
           PI_ADDR, PI_PORT, RED_PIN, GREEN_PIN, BLUE_PIN, SHARED_SECRET, OPENRGB_SERVER, OPENRGB_PORT);
#endif
    config_destroy(&cfg);
    return 0;
}

void parse_args(int argc, char *argv[]) {
    int opt;
    static struct option long_options[] = {{"server", required_argument, 0, 's'},
                                           {"port", required_argument, 0, 'p'},
                                           {"RED", required_argument, 0, 'R'},
                                           {"GREEN", required_argument, 0, 'G'},
                                           {"BLUE", required_argument, 0, 'B'},
                                           {"SHARED_SECRET", required_argument, 0, 'S'},
                                           {"OPENRGB_SERVER", required_argument, 0, 'O'},
                                           {"OPENRGB_PORT", required_argument, 0, 'P'},
                                           {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "c:s:p:R:G:B:S:O:P:", long_options, NULL)) != -1) {
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
        case 'O': {
            snprintf(OPENRGB_SERVER, sizeof(OPENRGB_SERVER), "%s", optarg);
            logger("OpenRGB server address set to: %s", OPENRGB_SERVER);
            break;
        }
        case 'P': {
            OPENRGB_PORT = atoi(optarg);
            logger("OpenRGB Server port set to: %d", OPENRGB_PORT);
            break;
        }
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

    if (try_load_config("/etc/piled/piled.conf") != 0) {
        logger("Can't load any of the configs! Aborting.");
        return -1;
    }

    return 0;
}