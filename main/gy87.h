#ifndef GY87_H
#define GY87_H

typedef enum {
    // defines a new enumeration type, code more readable
    TURN_NONE = 0,
    TURN_LEFT,
    TURN_RIGHT
} turn_direction_t;

// function declare
void gy87_main(void);
int GY87_init(void);
int GY87_read_gyro_z(float *gyro_z);
turn_direction_t GY87_detect_turn(float gyro_z);

#endif
