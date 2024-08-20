#include "gpio.h"
#include "pigpiod_if2.h"
#include "utils.h"
#include <stdint.h>
#include <unistd.h>

void set_color(int pi, struct Color color, uint8_t duration) {
    logger("set_color: Setting colors: %d %d %d on RPi #%d", color.RED, color.GREEN, color.BLUE, pi);
    logger("set_color: duration is %d seconds.", duration);
    if(duration == 0) {
        set_PWM_dutycycle(pi, RED_PIN, color.RED);
        set_PWM_dutycycle(pi, GREEN_PIN, color.GREEN);
        set_PWM_dutycycle(pi, BLUE_PIN, color.BLUE);
    } else {
        uint8_t red_step_size = color.RED - last_color.RED;
        uint8_t green_step_size = color.GREEN - last_color.GREEN;
        uint8_t blue_step_size = color.BLUE - last_color.BLUE;

        uint32_t step_duration_us = (duration * 1000000) / TRANSITION_STEPS;

        for(uint8_t step = 0; step < TRANSITION_STEPS; step++) {
            set_PWM_dutycycle(pi, RED_PIN, last_color.RED + (red_step_size * step) / TRANSITION_STEPS);
            set_PWM_dutycycle(pi, GREEN_PIN, last_color.GREEN + (green_step_size * step) / TRANSITION_STEPS);
            set_PWM_dutycycle(pi, BLUE_PIN, last_color.BLUE + (blue_step_size * step) / TRANSITION_STEPS);
            usleep(step_duration_us);
        }
        set_PWM_dutycycle(pi, RED_PIN, color.RED);
        set_PWM_dutycycle(pi, GREEN_PIN, color.GREEN);
        set_PWM_dutycycle(pi, BLUE_PIN, color.BLUE);
    }
    last_color = color;
}