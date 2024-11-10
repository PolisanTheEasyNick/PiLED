#include "gpio.h"
#include "../globals/globals.h"
#include "../server/server.h"
#include "../utils/utils.h"
#include "openrgb.h"
#include "pigpiod_if2.h"
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

pthread_t animation_thread;
uint8_t is_animating = 0;
pthread_mutex_t animation_mutex = PTHREAD_MUTEX_INITIALIZER;

void set_color(int pi, struct Color color) {
    logger_debug("set_color: Setting colors: %d %d %d on RPi #%d", color.RED, color.GREEN, color.BLUE, pi);
    set_PWM_dutycycle(pi, RED_PIN, color.RED);
    set_PWM_dutycycle(pi, GREEN_PIN, color.GREEN);
    set_PWM_dutycycle(pi, BLUE_PIN, color.BLUE);

    openrgb_set_color_on_devices(color);
    // will spam at animation :3
    // but will be synced with UI on clients
    // sending info about new color to all clients
    send_info_about_color();
}

void set_color_duration(int pi, struct Color color, uint8_t duration) {
    stop_animation();
    set_color_duration_anim(pi, color, duration);
}

void set_color_duration_anim(int pi, struct Color color, uint8_t duration) {
    logger_debug("set_color_duration: Setting colors: %d %d %d on RPi #%d", color.RED, color.GREEN, color.BLUE, pi);
    logger_debug("set_color_duration: duration is %d seconds.", duration);
    if (duration == 0) {
        set_color(pi, color);
    } else {
        is_animating = 1;
        struct Color last_color = {get_PWM_dutycycle(pi, RED_PIN), get_PWM_dutycycle(pi, GREEN_PIN),
                                   get_PWM_dutycycle(pi, BLUE_PIN)};
        logger_debug("set_color_duration: Got last color %d %d %d", last_color.RED, last_color.GREEN, last_color.BLUE);
        short red_step_size = color.RED - last_color.RED;
        short green_step_size = color.GREEN - last_color.GREEN;
        short blue_step_size = color.BLUE - last_color.BLUE;

        logger_debug("set_color_duration: Calculated step sizes %d %d %d", red_step_size, green_step_size,
                     blue_step_size);

        uint32_t step_duration_us = (duration * 1000000) / TRANSITION_STEPS;

        for (uint8_t step = 0; step < TRANSITION_STEPS; step++) {
            if (!is_animating)
                return;
            logger_debug("set_color_duration: step #%d", step);
            short red = last_color.RED + (red_step_size * step) / TRANSITION_STEPS;
            short green = last_color.GREEN + (green_step_size * step) / TRANSITION_STEPS;
            short blue = last_color.BLUE + (blue_step_size * step) / TRANSITION_STEPS;
            logger_debug("set_color_duration: setting color %d %d %d", red, green, blue);
            set_color(pi, (struct Color){red, green, blue});
            usleep(step_duration_us);
        }
        set_color(pi, color);
    }
}

void fade_out(int pi, uint8_t color_pin, uint8_t speed) {
    for (int i = get_PWM_dutycycle(pi, color_pin); i < 255; i++) {
        pthread_mutex_lock(&animation_mutex);
        if (!is_animating || stop_server) {
            pthread_mutex_unlock(&animation_mutex);
            return;
        }
        pthread_mutex_unlock(&animation_mutex);

        set_PWM_dutycycle(pi, color_pin, i);
        struct Color cur_color = {get_PWM_dutycycle(pi, RED_PIN), get_PWM_dutycycle(pi, GREEN_PIN),
                                  get_PWM_dutycycle(pi, BLUE_PIN)};
        openrgb_set_color_on_devices(cur_color);
        send_info_about_color();
        usleep(5000 / speed);
    }
}

void fade_in(int pi, uint8_t color_pin, uint8_t speed) {
    for (int i = get_PWM_dutycycle(pi, color_pin); i > 0; i--) {
        pthread_mutex_lock(&animation_mutex);
        if (!is_animating || stop_server) {
            pthread_mutex_unlock(&animation_mutex);
            return;
        }
        pthread_mutex_unlock(&animation_mutex);

        set_PWM_dutycycle(pi, color_pin, i);
        struct Color cur_color = {get_PWM_dutycycle(pi, RED_PIN), get_PWM_dutycycle(pi, GREEN_PIN),
                                  get_PWM_dutycycle(pi, BLUE_PIN)};
        openrgb_set_color_on_devices(cur_color);
        send_info_about_color();
        usleep(5000 / speed);
    }
}

int check_to_stop_anim() {
    logger_debug("Checking whether to stop animation...");
    pthread_mutex_lock(&animation_mutex);
    if (!is_animating || stop_server) {
        pthread_mutex_unlock(&animation_mutex);
        logger_debug("Stopping animation...");
        return 1;
    }
    pthread_mutex_unlock(&animation_mutex);
    logger_debug("Not stopping animation.");
    return 0;
}

void *start_fade_animation(void *arg) {
    pthread_mutex_lock(&animation_mutex);
    is_animating = 1;
    pthread_mutex_unlock(&animation_mutex);

    struct fade_animation_args *args = (struct fade_animation_args *)arg;
    int pi = args->pi;
    uint8_t speed = args->speed;

    while (1) {
        if (check_to_stop_anim())
            break;

        logger_debug("Animating fade with speed %d...", speed);
        fade_in(pi, RED_PIN, speed);
        if (check_to_stop_anim())
            break;
        fade_in(pi, BLUE_PIN, speed);
        if (check_to_stop_anim())
            break;
        fade_out(pi, RED_PIN, speed);
        if (check_to_stop_anim())
            break;
        fade_in(pi, GREEN_PIN, speed);
        if (check_to_stop_anim())
            break;
        fade_out(pi, BLUE_PIN, speed);
        if (check_to_stop_anim())
            break;
        fade_in(pi, RED_PIN, speed);
        if (check_to_stop_anim())
            break;
        fade_out(pi, GREEN_PIN, speed);
        if (check_to_stop_anim())
            break;
        fade_out(pi, RED_PIN, speed);
        if (check_to_stop_anim())
            break;
    }
    pthread_exit(NULL);
}

void *start_pulse_animation(void *arg) {
    pthread_mutex_lock(&animation_mutex);
    is_animating = 1;
    pthread_mutex_unlock(&animation_mutex);

    struct pulse_animation_args *args = (struct pulse_animation_args *)arg;
    int pi = args->pi;
    struct Color color = args->color;
    uint8_t duration = args->duration;

    set_color_duration_anim(pi, color, 3);
    logger_debug("Before while..... is_anim: %d, is stop server: %d", is_animating, stop_server);

    while (1) {
        if (check_to_stop_anim())
            break;
        pthread_mutex_lock(&animation_mutex);
        if (!is_animating || stop_server) {
            pthread_mutex_unlock(&animation_mutex);
            break;
        }
        pthread_mutex_unlock(&animation_mutex);

        logger_debug("Animating PULSE with duration %d...", duration);
        set_color_duration_anim(pi, (struct Color){0, 0, 0}, duration);
        if (check_to_stop_anim())
            break;
        usleep(1000);
        if (check_to_stop_anim())
            break;
        set_color_duration_anim(pi, color, duration);
        if (check_to_stop_anim())
            break;
        usleep(500);
        if (check_to_stop_anim())
            break;
    }
    pthread_exit(NULL);
}