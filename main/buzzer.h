#ifndef BUZZER_H
#define BUZZER_H

// Active buzzer: ON/OFF only (simple beep).
// Passive buzzer: used for secondary cues
// Active is small, Passive is big

#define ACTIVE_BUZZER_PIN 18 // ran pin
#define PASSIVE_BUZZER_PIN 19

// Call startup
void buzzer_init(void);

// sensor val
void buzzer_set_distance_cm(int distance_cm, int is_valid);
void buzzer_set_direction(int direction);
void buzzer_set_mode_event(int event_code);
void buzzer_set_error(int has_error);

// Call repeatedly in the main loop
void buzzer_iteration(void);
void buzzer_iteration_main(int err, int event, int direction_arg,
                           int distance_cm);

#endif
