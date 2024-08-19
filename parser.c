#include "parser.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <openssl/hmac.h>

void parse_message(unsigned char buffer[BUFFER_SIZE]) {
    logger("Received buffer: ");
    for(int i = 0; i < BUFFER_SIZE; i++) {
        printf("%x ", buffer[i]);
    }
    //8 first bytes is timestamp
    uint64_t timestamp = 0;
    for(unsigned short i = 0; i < 8; i++) {
        timestamp |= buffer[i] & 0xFF;
        if(i != 7) timestamp <<= 8;
    }
    logger("Extracted timestamp %lu: 0x%lx\n", timestamp, timestamp);

    uint64_t current_time = time(NULL); //must be 64-bit on modern systems
    logger("Current timestamp: %lu", current_time);
    if(current_time - timestamp <= 5) {
        logger("The timestamp is within allowed time difference of 5 seconds.");
    } else {
        logger("Error! Timestamp difference is too big (%d seconds.)! Aborting.", current_time-timestamp);
        //return;
    }

    //next 8 bytes -> nonce
    uint64_t nonce = 0;
    for(unsigned short i = 8; i < 16; i++) {
        nonce |= buffer[i] & 0xFF;
        if(i != 15) nonce <<= 8;
    }
    logger("nonce: 0x%lx\n", nonce);

    //VERSION
    uint8_t version = 0;
    version |= buffer[16] & 0xFF;
    logger("version: 0x%lx; v%d\n", version, version);

    //HMAC
    unsigned char PARSED_HMAC[32];
    memcpy(PARSED_HMAC, &buffer[17], 32);
    logger("HMAC:");
    for(unsigned short i = 0; i < 32; i++) {
        printf("%x ", PARSED_HMAC[i]);
    }
    printf("\n");

    //PAYLOAD
    uint8_t RED = 0, GREEN = 0, BLUE = 0;
    if(version == 0x01) {
        RED |= buffer[49] & 0xFF;
        GREEN |= buffer[50] & 0xFF;
        BLUE |= buffer[51] & 0xFF;
    }
    logger("Color:\nR: 0x%x, G: 0x%x, B: 0x%x", RED, GREEN, BLUE);
    if(version == 0x01) { //for future versions
        #define HMAC_DATA_SIZE 20 //header + payload size
    } else { //backporting to ver 1, except errors 
        #define HMAC_DATA_SIZE 20
    }
    unsigned char HMAC_DATA[HMAC_DATA_SIZE];
    memset(HMAC_DATA, 0, HMAC_DATA_SIZE);
    memcpy(HMAC_DATA, buffer, 17);
    memcpy(&HMAC_DATA[17], &buffer[49], 3);

    logger("HEADER + PAYLOAD:");
    for(int i = 0; i < HMAC_DATA_SIZE; i++) {
        printf("%x ", HMAC_DATA[i]);
    }
    printf("\n");

    //generating new hmac
    char *key = SHARED_SECRET;
    int key_length = strlen(key);
    unsigned char GENERATED_HMAC[32];
    unsigned int hmac_len;
    HMAC(EVP_sha256(), key, key_length, HMAC_DATA, HMAC_DATA_SIZE, GENERATED_HMAC, &hmac_len);
    printf("Generated HMAC length: %d, HMAC:\n", hmac_len);
    for(int i = 0; i < 32; i++) {
        printf("%x ", GENERATED_HMAC[i]);
    }
    printf("\n");
    logger("Comparing HMAC's...");
    for(unsigned short i = 0; i < 32; i++) {
        if(GENERATED_HMAC[i] != PARSED_HMAC[i]) {
            logger("HMACs are NOT the sa-*kabooom*");
            return;
        }
    }
    logger("HMACs are same, nothing exploded!");

}