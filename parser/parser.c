#include "parser.h"
#include "../rgb/openrgb.h"
#include "../utils/utils.h"
#include <libconfig.h>
#include <openssl/hmac.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct parse_result parse_payload(unsigned char *buffer, const uint8_t version, unsigned char *PARSED_HMAC) {
    // PAYLOAD
    uint8_t RED = 0, GREEN = 0, BLUE = 0, duration = 0, speed = 0;

    struct section_sizes sizes = get_section_sizes(version);
    uint8_t payload_offset = sizes.header_size + 32;

    RED |= buffer[payload_offset] & 0xFF;
    GREEN |= buffer[payload_offset + 1] & 0xFF;
    BLUE |= buffer[payload_offset + 2] & 0xFF;
    if (version >= 2) {
        duration |= buffer[payload_offset + 3] & 0xFF;
        logger("parse_message: Version: %d, got duration: %d", version, duration);
    }
    if (version >= 4) {
        speed |= buffer[payload_offset + 4] & 0xFF;
        logger("parse_message: Version: %d, got speed: %d", version, speed);
    }

    logger("parse_message: Color: R: 0x%x, G: 0x%x, B: 0x%x", RED, GREEN, BLUE);
    const int HMAC_DATA_SIZE = sizes.header_size + sizes.payload_size;
    logger("parse_message: data for hmac size: %d; payload size: %d", HMAC_DATA_SIZE, sizes.payload_size);
    unsigned char HMAC_DATA[HMAC_DATA_SIZE];
    memset(HMAC_DATA, 0, HMAC_DATA_SIZE);
    memcpy(HMAC_DATA, buffer, sizes.header_size);                                       // HEADER
    memcpy(&HMAC_DATA[sizes.header_size], &buffer[payload_offset], sizes.payload_size); // PAYLOAD

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
    res.speed = speed;
    return res;
}

struct parse_result parse_message(unsigned char buffer[BUFFER_SIZE]) {
    logger("parse_message: received buffer: ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        printf("%x ", buffer[i]);
    }
    printf("\n");
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

    struct section_sizes sizes = get_section_sizes(version);

    uint8_t OP = 0;
    if (version >= 3) {
        OP = buffer[sizes.header_size - 1] & 0xFF;
        logger("parse_message: Operational code is: 0x%x", OP);
    }

    // HMAC
    unsigned char PARSED_HMAC[32];
    memcpy(PARSED_HMAC, &buffer[sizes.header_size], 32);
    logger("parse_message: Parsed HMAC from buffer:");
    for (unsigned short i = 0; i < 32; i++) {
        printf("%x ", PARSED_HMAC[i]);
    }
    printf("\n");
    struct parse_result result;
    switch (OP) {
    case LED_SET_COLOR: { // SET COLOR
        logger("parse_message: Operational code is 0, setting color");
        result = parse_payload(buffer, version, PARSED_HMAC);
        result.OP = LED_SET_COLOR;
        break;
    };
    case LED_GET_CURRENT_COLOR: { // GET COLOR
        // TODO: HMAC check?? No point since get color request not sensible?
        logger("parse_message: OP code is 1, getting color");
        result.result = 0;
        result.OP = LED_GET_CURRENT_COLOR;
        result.version = 3; // minimal for this OP; change logic in future?
        break;
    }
    case ANIM_SET_FADE: {
        logger("parse_message: OP code is 2, starting FADE animation");
        result = parse_payload(buffer, version, PARSED_HMAC);
        result.OP = ANIM_SET_FADE;
        result.version = 4;
        break;
    }
    case ANIM_SET_PULSE: {
        logger("parse_message: OP code is 3, setting PULSE animation");
        result = parse_payload(buffer, version, PARSED_HMAC);
        result.OP = ANIM_SET_PULSE;
        result.version = 4;
        break;
    }
    default: {
        logger("parse_message: Unknown OP (%d), aborting!", OP);
        result.result = 1;
        break;
    }
    }
    return result;
}

uint8_t parse_config(const char *config_file) {
    config_t cfg;
    config_init(&cfg);

    if (!config_read_file(&cfg, config_file)) {
        fprintf(stderr, "Error reading config file: %s\n", config_error_text(&cfg));
        config_destroy(&cfg);
        return 1;
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
    return 0;
}

struct section_sizes get_section_sizes(uint8_t version) {
    struct section_sizes result;
    result.header_size = HEADER_SIZE;
    result.payload_size = PAYLOAD_SIZE;
    switch (version) {
    case 1: { // no OP, no duration
        result.header_size = 17;
        result.payload_size = 3;
        break;
    }
    case 2: { // duration added
        result.header_size = 17;
        result.payload_size = 4;
        break;
    }
    case 3: { // OP added
        result.header_size = 18;
        result.payload_size = 4;
    }
    case 4: { // Speed added to PAYLOAD
        result.header_size = 18;
        result.payload_size = 5;
    }
    }
    return result;
}

void parse_openrgb_response(const uint8_t *buffer, uint32_t buffer_size) {
    logger("OpenRGB Parser: Starting parsing buffer with size %d", buffer_size);
    printf("Buffer:\n");
    for (int i = 0; i < buffer_size; i++) {
        printf("%x ", buffer[i]);
    }
    printf("\n");

    // Checking for magick
    if (buffer[0] != 'O' || buffer[1] != 'R' || buffer[2] != 'G' || buffer[3] != 'B') {
        printf("Magick wrong! Not a OpenRGB package!\n");
        return;
    }

    // Getting device index
    uint32_t pkt_dev_idx = 0;
    memcpy(&pkt_dev_idx, buffer + 4, 4);
    printf("Got device index: %d\n", pkt_dev_idx);

    uint32_t pkt_id = 0;
    memcpy(&pkt_id, buffer + 8, 4);
    printf("Got packet id: %d\n", pkt_id);

    uint32_t pkt_size = 0;
    memcpy(&pkt_size, buffer + 12, 4);
    printf("Got packet size: %d\n", pkt_size);

    switch (pkt_id) {
    case OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_COUNT: {
        printf("Packet id is OpenRGB response controller count (response)\n");
        // TODO: fetch controller count
        break;
    }
    case OPENRGB_NET_PACKET_ID_REQUEST_CONTROLLER_DATA: {
        printf("NET_PACKET_ID_REQUEST_CONTROLLER_DATA not implemented.\n");
        // Do i need it for this project?
        break;
    }
    case OPENRGB_NET_PACKET_ID_REQUEST_PROTOCOL_VERSION: {
        printf("Packet id is NET_PACKET_ID_REQUEST_PROTOCOL_VERSION\n");
        if (pkt_size != 4) {
            printf("Packet size is not 4!\n");
            return;
        }
        uint32_t openrgb_version = 0;
        memcpy(&openrgb_version, buffer + 16, 4);
        openrgb_using_version =
            openrgb_version <= OPENRGB_SUPPORTED_VERSION ? openrgb_version : OPENRGB_SUPPORTED_VERSION;
        printf("OpenRGB Server's Version: %d, Client max supported version: %d, Using version: %d\n", openrgb_version,
               OPENRGB_SUPPORTED_VERSION, openrgb_using_version);
        break;
    }
    }
}