#include "parser.h"
#include "../utils/utils.h"
#include <libconfig.h>
#include <openssl/hmac.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
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
        logger_debug("parse_message: Version: %d, got duration: %d", version, duration);
    }
    if (version >= 4) {
        speed |= buffer[payload_offset + 4] & 0xFF;
        logger_debug("parse_message: Version: %d, got speed: %d", version, speed);
    }

    logger_debug("parse_message: Color: R: 0x%x, G: 0x%x, B: 0x%x", RED, GREEN, BLUE);
    const int HMAC_DATA_SIZE = sizes.header_size + sizes.payload_size;
    logger_debug("parse_message: data for hmac size: %d; payload size: %d", HMAC_DATA_SIZE, sizes.payload_size);
    unsigned char HMAC_DATA[HMAC_DATA_SIZE];
    memset(HMAC_DATA, 0, HMAC_DATA_SIZE);
    memcpy(HMAC_DATA, buffer, sizes.header_size);                                       // HEADER
    memcpy(&HMAC_DATA[sizes.header_size], &buffer[payload_offset], sizes.payload_size); // PAYLOAD

#ifdef DEBUG
    logger_debug("parse_message: HEADER + PAYLOAD:");
    for (int i = 0; i < HMAC_DATA_SIZE; i++) {
        printf("%x ", HMAC_DATA[i]);
    }
    printf("\n");
#endif

    // generating new hmac
    char *key = SHARED_SECRET;
    int key_length = strlen(key);
    unsigned char GENERATED_HMAC[32];
    unsigned int hmac_len;
    HMAC(EVP_sha256(), key, key_length, HMAC_DATA, HMAC_DATA_SIZE, GENERATED_HMAC, &hmac_len);
#ifdef DEBUG
    printf("parse_message: Generated HMAC length: %d, HMAC:\n", hmac_len);
    for (int i = 0; i < 32; i++) {
        printf("%x ", GENERATED_HMAC[i]);
    }
    printf("\n");
#endif

#ifndef DEBUG
    logger_debug("parse_message: Comparing HMAC's...");
    for (unsigned short i = 0; i < 32; i++) {
        if (GENERATED_HMAC[i] != PARSED_HMAC[i]) {
            logger_debug("parse_message: HMACs are NOT the sa-*kabooom*");
            struct parse_result err;
            err.result = 1;
            return err;
        }
    }
    logger_debug("parse_message: HMACs are same, nothing exploded!");
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
#ifdef DEBUG
    logger_debug("parse_message: received buffer: ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        printf("%x ", buffer[i]);
    }
    printf("\n");
#endif
    // 8 first bytes is timestamp
    uint64_t timestamp = 0;
    for (unsigned short i = 0; i < 8; i++) {
        timestamp |= buffer[i] & 0xFF;
        if (i != 7)
            timestamp <<= 8;
    }
    logger_debug("parse_message: extracted timestamp %lu: 0x%lx\n", timestamp, timestamp);

    uint64_t current_time = time(NULL); // must be 64-bit on modern systems
    logger_debug("parse_message: current timestamp: %lu", current_time);

#ifndef DEBUG
    unsigned short difference;
    if (current_time > timestamp) {
        difference = current_time - timestamp;
    } else {
        difference = timestamp - current_time;
    }

    if (difference <= 5) {
        logger_debug("parse_message: the timestamp is within allowed time difference of "
                     "5 seconds.");
    } else {
        logger_debug("parse_message: Error! Timestamp difference is too big (%d "
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
    logger_debug("parse_message: nonce: 0x%lx\n", nonce);

    // VERSION
    uint8_t version = 0;
    version |= buffer[16] & 0xFF;
    logger_debug("parse_message: version: 0x%lx; v%d\n", version, version);

    struct section_sizes sizes = get_section_sizes(version);

    uint8_t OP = 0;
    if (version >= 3) {
        OP = buffer[sizes.header_size - 1] & 0xFF;
        logger_debug("parse_message: Operational code is: 0x%x", OP);
    }

    // HMAC
    unsigned char PARSED_HMAC[32];
    memcpy(PARSED_HMAC, &buffer[sizes.header_size], 32);
#ifdef DEBUG
    logger("parse_message: Parsed HMAC from buffer:");
    for (unsigned short i = 0; i < 32; i++) {
        printf("%x ", PARSED_HMAC[i]);
    }
    printf("\n");
#endif
    struct parse_result result;
    switch (OP) {
    case LED_SET_COLOR: { // SET COLOR
        logger_debug("parse_message: Operational code is 0, setting color");
        result = parse_payload(buffer, version, PARSED_HMAC);
        result.OP = LED_SET_COLOR;
        break;
    };
    case LED_GET_CURRENT_COLOR: { // GET COLOR
        // TODO: HMAC check?? No point since get color request not sensible?
        logger_debug("parse_message: OP code is LED_GET_CURRENT_COLOR, getting color");
        result.result = 0;
        result.OP = LED_GET_CURRENT_COLOR;
        result.version = 3; // minimal for this OP; change logic in future?
        break;
    }
    case ANIM_SET_FADE: {
        logger_debug("parse_message: OP code is ANIM_SET_FADE, starting FADE animation");
        result = parse_payload(buffer, version, PARSED_HMAC);
        result.OP = ANIM_SET_FADE;
        result.version = 4;
        break;
    }
    case ANIM_SET_PULSE: {
        logger_debug("parse_message: OP code is ANIM_SET_PULSE, setting PULSE animation");
        result = parse_payload(buffer, version, PARSED_HMAC);
        result.OP = ANIM_SET_PULSE;
        result.version = 4;
        break;
    }
    case SYS_TOGGLE_SUSPEND: {
        logger_debug("parse_message: OP code is SYS_TOGGLE_SUSPEND.");
        result = parse_payload(buffer, version, PARSED_HMAC);
        result.OP = SYS_TOGGLE_SUSPEND;
        result.version = 4;
        break;
    }
    default: {
        logger_debug("parse_message: Unknown OP (%d), aborting!", OP);
        result.result = 1;
        break;
    }
    }
    return result;
}

int count_valid_lines(const char *config_file) {
    FILE *file = fopen(config_file, "r");
    if (file == NULL) {
        logger("Failed to open config file");
        return -1;
    }

    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '#') {
            count++;
        }
    }

    fclose(file);
    return count;
}

void parse_openrgb_config_devices(const char *config_file) {
    int device_count = count_valid_lines(config_file);
    if (device_count <= 0) {
        logger("No valid devices found in the config file. Re/Create OpenRGB config using openrgb_configurator!\n");
        return;
    }
    openrgb_using_devices_num = device_count;

    openrgb_devices_to_change = malloc(sizeof(struct openrgb_device) * device_count);

    FILE *file = fopen(config_file, "r");
    if (file == NULL) {
        logger("Failed to open config file");
        return;
    }

    uint8_t line[256];
    uint8_t device = 0;
    while (fgets((char *)line, sizeof(line), file) != NULL) {
        if (line[0] != '#') {
            continue;
        }

        uint8_t *content = line + 1;
        while (*content == ' ' || *content == '\t')
            content++;

        uint8_t *colon_pos = (uint8_t *)strchr((char *)content, ':');
        if (colon_pos == NULL) {
            logger("Invalid line format: %s", line);
            continue;
        }

        *colon_pos = '\0';
        int number = atoi((const char *)content);

        uint8_t *name = colon_pos + 1;
        while (*name == ' ' || *name == '\t')
            name++;

        uint8_t *newline = (uint8_t *)strchr((char *)name, '\n');
        if (newline != NULL)
            *newline = '\0';
        openrgb_devices_to_change[device].device_id = number;
        openrgb_devices_to_change[device].name = malloc(strlen((char *)name) + 1);
        strcpy((char *)openrgb_devices_to_change[device++].name, (char *)name);
    }

    fclose(file);
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