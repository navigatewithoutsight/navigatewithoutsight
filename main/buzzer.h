#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>

/* Pins (per TOC hardware mapping) */
#define ACTIVE_BUZZER_PIN  25
#define PASSIVE_BUZZER_PIN 26

/* Init both buzzers (active = GPIO, passive = PWM) */
void buzzer_init(void);

/* Update latest distance (accepts cm or mm; implementation auto-detects) */
void buzzer_set_distance_cm(int distance_value, int is_valid);

/* Optional direction cue (-1 = left, 0 = none, +1 = right) */
void buzzer_set_direction(int direction);

/* One-shot event tone on passive buzzer (e.g., startup/mode change) */
void buzzer_set_mode_event(int event_code);

/* Force error pattern (1 = error, 0 = normal) */
void buzzer_set_error(int has_error);

/* Call often (non-blocking) */
void buzzer_iteration(void);

/* Wrapper used by real_main.c */
void buzzer_iteration_main(int err, int event, int direction_arg, int distance_mm);

#endif
