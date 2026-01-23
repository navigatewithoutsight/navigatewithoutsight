#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

/* Use plain integer for GPIO to avoid driver dependency in header */
typedef struct {
    int gpio;
    uint32_t debounce_ms;
} button_config_t;

/* Initialize button */
void button_init(const button_config_t *cfg);

/* Returns true once per press */
bool button_was_pressed(void);

#endif
