#ifndef GPIO_H
#define GPIO_H

#include <pthread.h>
#include <stdint.h>

#define TRANSITION_STEPS 100

struct Color {
    uint8_t RED;
    uint8_t GREEN;
    uint8_t BLUE;
};

struct fade_animation_args {
    int pi;
    uint8_t speed;
};

void set_color(int pi, struct Color color);
void set_color_duration(int pi, struct Color color, uint8_t duration);
void *start_fade_animation(void *arg);

extern pthread_t fade_animation_thread;
extern uint8_t is_animating;
extern pthread_mutex_t animation_mutex;

#endif // GPIO_H