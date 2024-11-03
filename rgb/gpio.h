#ifndef GPIO_H
#define GPIO_H

#include "../utils/utils.h"
#include <pthread.h>
#include <stdint.h>

#define TRANSITION_STEPS 100

struct fade_animation_args {
    int pi;
    uint8_t speed;
};

struct pulse_animation_args {
    int pi;
    struct Color color;
    uint8_t duration;
};

// operational functions
void set_color(int pi, struct Color color);
void set_color_duration_anim(int pi, struct Color color, uint8_t duration);
void set_color_duration(int pi, struct Color color, uint8_t duration);

// animations
void *start_fade_animation(void *arg);
void *start_pulse_animation(void *arg);

extern pthread_t animation_thread;
extern uint8_t is_animating;
extern pthread_mutex_t animation_mutex;

#endif // GPIO_H