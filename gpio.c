#include "gpio.h"
#include "globals.h"
#include "pigpiod_if2.h"
#include "utils.h"
#include <stdint.h>
#include <unistd.h>

void set_color(int pi, struct Color color, uint8_t duration) {
    logger("set_color: Setting colors: %d %d %d on RPi #%d", color.RED, color.GREEN, color.BLUE, pi);
    logger("set_color: duration is %d seconds.", duration);
    if (duration == 0) {
        set_PWM_dutycycle(pi, RED_PIN, color.RED);
        set_PWM_dutycycle(pi, GREEN_PIN, color.GREEN);
        set_PWM_dutycycle(pi, BLUE_PIN, color.BLUE);
    } else {
        struct Color last_color = {get_PWM_dutycycle(pi, RED_PIN), get_PWM_dutycycle(pi, GREEN_PIN),
                                   get_PWM_dutycycle(pi, BLUE_PIN)};
        logger("set_color: Got last color %d %d %d", last_color.RED, last_color.GREEN, last_color.BLUE);
        short red_step_size = color.RED - last_color.RED;
        short green_step_size = color.GREEN - last_color.GREEN;
        short blue_step_size = color.BLUE - last_color.BLUE;

        logger("set_color: Calculated step sizes %d %d %d", red_step_size, green_step_size, blue_step_size);

        uint32_t step_duration_us = (duration * 1000000) / TRANSITION_STEPS;

        for (uint8_t step = 0; step < TRANSITION_STEPS; step++) {
            logger("set_color: step #%d", step);
            short red = last_color.RED + (red_step_size * step) / TRANSITION_STEPS;
            short green = last_color.GREEN + (green_step_size * step) / TRANSITION_STEPS;
            short blue = last_color.BLUE + (blue_step_size * step) / TRANSITION_STEPS;
            logger("set_color: setting color %d %d %d", red, green, blue);
            set_PWM_dutycycle(pi, RED_PIN, red);
            set_PWM_dutycycle(pi, GREEN_PIN, green);
            set_PWM_dutycycle(pi, BLUE_PIN, blue);
            usleep(step_duration_us);
        }
        set_PWM_dutycycle(pi, RED_PIN, color.RED);
        set_PWM_dutycycle(pi, GREEN_PIN, color.GREEN);
        set_PWM_dutycycle(pi, BLUE_PIN, color.BLUE);
    }
}