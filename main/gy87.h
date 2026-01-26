#ifndef GY87_H
#define GY87_H
#include "freertos/idf_additions.h"

typedef enum {
  // defines a new enumeration type, code more readable
  TURN_NONE = 0,
  TURN_LEFT,
  TURN_RIGHT
} turn_direction_t;

// function declare
void gy87_main(void);
int GY87_init(void);
int GY87_init_no_i2c_bus(void);
int GY87_read_gyro_z(float *gyro_z, SemaphoreHandle_t mutex);
void gy87_loop_iteration(float *cadence_arg, float *speed_arg, float *gz,
                         SemaphoreHandle_t mutex);
turn_direction_t GY87_detect_turn(float gyro_z);
int GY87_init_no_i2c_bus(void);

void GY87_update_cadence_and_speed(float *cadence, float *speed,
                                   SemaphoreHandle_t mutex);

void analize_gz(float *gz_arg, int *turn_direction_arg, int *turn_degree_arg);
#endif
