#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

uint8_t parse_config(const char *config_file);
void parse_args(int argc, char *argv[]);
uint8_t try_load_config(const char *config_path);
int load_config();

#endif