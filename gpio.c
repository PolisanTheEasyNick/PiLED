#include "gpio.h"
#include "pigpiod_if2.h"
#include "utils.h"

void set_color(int pi, uint8_t RED, uint8_t GREEN, uint8_t BLUE) {
    logger("Setting colors: %d %d %d on RPi #%d", RED, GREEN, BLUE, pi);
    set_PWM_dutycycle(pi, RED_PIN, RED);
    set_PWM_dutycycle(pi, GREEN_PIN, GREEN);
    set_PWM_dutycycle(pi, BLUE_PIN, BLUE);
}