#ifndef PARSER_H
#define PARSER_H
#include "globals.h"
#include <libconfig.h>
#include <stdint.h>

struct parse_result {
    unsigned short result; // 0 if success, 1 on errors
    uint8_t version;       // protocol version
    uint8_t RED;
    uint8_t GREEN;
    uint8_t BLUE;
    uint8_t duration;
    uint8_t OP;    // OPerational code
    uint8_t speed; // for animations
};

struct section_sizes {
    unsigned short header_size;
    unsigned short payload_size;
};

struct parse_result parse_message(unsigned char buffer[BUFFER_SIZE]);
void parse_config(const char *config_file);
struct section_sizes get_section_sizes(uint8_t version);

#endif // PARSER_H