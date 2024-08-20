#ifndef GPIO_H
#define GPIO_H
#include "utils.h"
#include <stdint.h>

#define TRANSITION_STEPS 100

struct Color {
    uint8_t RED;
    uint8_t GREEN;
    uint8_t BLUE;
};

static struct Color last_color = {0, 0, 0};

void set_color(int pi, struct Color color, uint8_t duration);

#endif //GPIO_H