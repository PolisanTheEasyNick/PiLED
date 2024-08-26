#include "gpio.h"
#include "globals.h"
#include "pigpiod_if2.h"
#include "server.h"
#include "utils.h"
#include <stdint.h>
#include <unistd.h>

pthread_t fade_animation_thread;
uint8_t is_animating = 0;
pthread_mutex_t animation_mutex = PTHREAD_MUTEX_INITIALIZER;

void set_color(int pi, struct Color color) {
    logger("set_color: Setting colors: %d %d %d on RPi #%d", color.RED, color.GREEN, color.BLUE, pi);
    set_PWM_dutycycle(pi, RED_PIN, color.RED);
    set_PWM_dutycycle(pi, GREEN_PIN, color.GREEN);
    set_PWM_dutycycle(pi, BLUE_PIN, color.BLUE);
}

void set_color_duration(int pi, struct Color color, uint8_t duration) {
    is_animating = 0; // stop animation if any
    logger("set_color_duration: Setting colors: %d %d %d on RPi #%d", color.RED, color.GREEN, color.BLUE, pi);
    logger("set_color_duration: duration is %d seconds.", duration);
    if (duration == 0) {
        set_PWM_dutycycle(pi, RED_PIN, color.RED);
        set_PWM_dutycycle(pi, GREEN_PIN, color.GREEN);
        set_PWM_dutycycle(pi, BLUE_PIN, color.BLUE);
    } else {
        struct Color last_color = {get_PWM_dutycycle(pi, RED_PIN), get_PWM_dutycycle(pi, GREEN_PIN),
                                   get_PWM_dutycycle(pi, BLUE_PIN)};
        logger("set_color_duration: Got last color %d %d %d", last_color.RED, last_color.GREEN, last_color.BLUE);
        short red_step_size = color.RED - last_color.RED;
        short green_step_size = color.GREEN - last_color.GREEN;
        short blue_step_size = color.BLUE - last_color.BLUE;

        logger("set_color_duration: Calculated step sizes %d %d %d", red_step_size, green_step_size, blue_step_size);

        uint32_t step_duration_us = (duration * 1000000) / TRANSITION_STEPS;

        for (uint8_t step = 0; step < TRANSITION_STEPS; step++) {
            logger("set_color_duration: step #%d", step);
            short red = last_color.RED + (red_step_size * step) / TRANSITION_STEPS;
            short green = last_color.GREEN + (green_step_size * step) / TRANSITION_STEPS;
            short blue = last_color.BLUE + (blue_step_size * step) / TRANSITION_STEPS;
            logger("set_color_duration: setting color %d %d %d", red, green, blue);
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

void fade_out(int pi, uint8_t color_pin, uint8_t speed) {
    for (int i = get_PWM_dutycycle(pi, color_pin); i < 255; i++) {
        if (!is_animating || stop_server)
            return;
        set_PWM_dutycycle(pi, color_pin, i);
        usleep(5000 / speed);
    }
}

void fade_in(int pi, uint8_t color_pin, uint8_t speed) {
    for (int i = get_PWM_dutycycle(pi, color_pin); i > 0; i--) {
        if (!is_animating || stop_server)
            return;
        set_PWM_dutycycle(pi, color_pin, i);
        usleep(5000 / speed);
    }
}

void *start_fade_animation(void *arg) {
    is_animating = 1;
    struct fade_animation_args *args = (struct fade_animation_args *)arg;
    int pi = args->pi;
    uint8_t speed = args->speed;
    while (is_animating && !stop_server) {
        logger("Animating with speed %d...", speed);
        fade_in(pi, RED_PIN, speed);
        fade_in(pi, BLUE_PIN, speed);
        fade_out(pi, RED_PIN, speed);
        fade_in(pi, GREEN_PIN, speed);
        fade_out(pi, BLUE_PIN, speed);
        fade_in(pi, RED_PIN, speed);
        fade_out(pi, GREEN_PIN, speed);
        fade_out(pi, RED_PIN, speed);
    }
    pthread_exit(NULL);
}