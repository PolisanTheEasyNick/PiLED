#include "parser.h"
#include "globals.h"
#include "utils.h"
#include <libconfig.h>
#include <openssl/hmac.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct parse_result parse_message(unsigned char buffer[BUFFER_SIZE]) {
    logger("parse_message: received buffer: ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        printf("%x ", buffer[i]);
    }
    // 8 first bytes is timestamp
    uint64_t timestamp = 0;
    for (unsigned short i = 0; i < 8; i++) {
        timestamp |= buffer[i] & 0xFF;
        if (i != 7)
            timestamp <<= 8;
    }
    logger("parse_message: extracted timestamp %lu: 0x%lx\n", timestamp, timestamp);

    uint64_t current_time = time(NULL); // must be 64-bit on modern systems
    logger("parse_message: current timestamp: %lu", current_time);

#ifndef DEBUG
    unsigned short difference;
    if (current_time > timestamp) {
        difference = current_time - timestamp;
    } else {
        difference = timestamp - current_time;
    }

    if (difference <= 5) {
        logger("parse_message: the timestamp is within allowed time difference of "
               "5 seconds.");
    } else {
        logger("parse_message: Error! Timestamp difference is too big (%d "
               "seconds.)! Aborting.",
               difference);
        struct parse_result err;
        err.result = 1;
        return err;
    }
#endif

    // next 8 bytes -> nonce
    uint64_t nonce = 0;
    for (unsigned short i = 8; i < 16; i++) {
        nonce |= buffer[i] & 0xFF;
        if (i != 15)
            nonce <<= 8;
    }
    logger("parse_message: nonce: 0x%lx\n", nonce);

    // VERSION
    uint8_t version = 0;
    version |= buffer[16] & 0xFF;
    logger("parse_message: version: 0x%lx; v%d\n", version, version);

    // HMAC
    unsigned char PARSED_HMAC[32];
    memcpy(PARSED_HMAC, &buffer[17], 32);
    logger("parse_message: HMAC:");
    for (unsigned short i = 0; i < 32; i++) {
        printf("%x ", PARSED_HMAC[i]);
    }
    printf("\n");

    // PAYLOAD
    uint8_t RED = 0, GREEN = 0, BLUE = 0, duration = 0;
    RED |= buffer[49] & 0xFF;
    GREEN |= buffer[50] & 0xFF;
    BLUE |= buffer[51] & 0xFF;
    if (version == 0x02) {
        duration |= buffer[52] & 0xFF;
    }
    logger("parse_message: Color:\nR: 0x%x, G: 0x%x, B: 0x%x", RED, GREEN, BLUE);
    const int HMAC_DATA_SIZE = (version == 0x02) ? 21 : 20;
    const int PAYLOAD_SIZE = (version == 0x02) ? 4 : 3;
    unsigned char HMAC_DATA[HMAC_DATA_SIZE];
    memset(HMAC_DATA, 0, HMAC_DATA_SIZE);
    memcpy(HMAC_DATA, buffer, 17);                     // HEADER
    memcpy(&HMAC_DATA[17], &buffer[49], PAYLOAD_SIZE); // PAYLOAD

    logger("parse_message: HEADER + PAYLOAD:");
    for (int i = 0; i < HMAC_DATA_SIZE; i++) {
        printf("%x ", HMAC_DATA[i]);
    }
    printf("\n");

    // generating new hmac
    char *key = SHARED_SECRET;
    int key_length = strlen(key);
    unsigned char GENERATED_HMAC[32];
    unsigned int hmac_len;
    HMAC(EVP_sha256(), key, key_length, HMAC_DATA, HMAC_DATA_SIZE, GENERATED_HMAC, &hmac_len);
    printf("parse_message: Generated HMAC length: %d, HMAC:\n", hmac_len);
    for (int i = 0; i < 32; i++) {
        printf("%x ", GENERATED_HMAC[i]);
    }
    printf("\n");

#ifndef DEBUG
    logger("parse_message: Comparing HMAC's...");
    for (unsigned short i = 0; i < 32; i++) {
        if (GENERATED_HMAC[i] != PARSED_HMAC[i]) {
            logger("parse_message: HMACs are NOT the sa-*kabooom*");
            struct parse_result err;
            err.result = 1;
            return err;
        }
    }
    logger("parse_message: HMACs are same, nothing exploded!");
#endif

    struct parse_result res;
    res.result = 0;
    res.version = version;
    res.RED = RED;
    res.GREEN = GREEN;
    res.BLUE = BLUE;
    res.duration = duration;
    return res;
}

void parse_config(const char *config_file) {
    config_t cfg;
    config_init(&cfg);

    if (!config_read_file(&cfg, config_file)) {
        fprintf(stderr, "Error reading config file: %s\n", config_error_text(&cfg));
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }
    logger("Opened config file");

    const char *addr;
    if (!config_lookup_string(&cfg, "PI_ADDR", &addr)) {
        fprintf(stderr, "Missing PI_ADDR in config file, using default (NULL)\n");
        PI_ADDR = NULL;
    } else {
        PI_ADDR = malloc(strlen(addr));
        strncpy(PI_ADDR, addr, strlen(addr));
        PI_ADDR[strlen(addr)] = 0;
    }

    const char *port;
    if (!config_lookup_string(&cfg, "PI_PORT", &port)) {
        fprintf(stderr, "Missing PI_PORT in config file, using default (8888)\n");
        PI_PORT = NULL;
    } else {
        PI_PORT = malloc(strlen(port));
        strncpy(PI_PORT, port, strlen(port));
        PI_PORT[strlen(port)] = 0;
    }

    if (!config_lookup_int(&cfg, "RED_PIN", &RED_PIN)) {
        fprintf(stderr, "Missing RED_PIN in config file!\n");
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }
    RED_PIN = RED_PIN;

    if (!config_lookup_int(&cfg, "GREEN_PIN", &GREEN_PIN)) {
        fprintf(stderr, "Missing GREEN_PIN in config file!\n");
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }
    GREEN_PIN = GREEN_PIN;

    if (!config_lookup_int(&cfg, "BLUE_PIN", &BLUE_PIN)) {
        fprintf(stderr, "Missing BLUE_PIN in config file!\n");
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }
    BLUE_PIN = BLUE_PIN;

    const char *secret;
    if (!config_lookup_string(&cfg, "SHARED_SECRET", &secret)) {
        fprintf(stderr, "Missing SHARED_SECRET in config file\n");
        SHARED_SECRET = NULL;
    }
    SHARED_SECRET = malloc(strlen(secret));
    strncpy(SHARED_SECRET, secret, strlen(secret));
    SHARED_SECRET[strlen(secret)] = 0;

    logger("Passed config:\nRaspberry Pi address: %s\nPort: %s\nRed pin: %d\nGreen pin: %d\nBlue pin: %d\nShared "
           "secret: %s",
           PI_ADDR, PI_PORT, RED_PIN, GREEN_PIN, BLUE_PIN, SHARED_SECRET);
}