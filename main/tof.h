#ifndef vl53l7cx_H
#define vl53l7cx_H
#include "freertos/idf_additions.h"
#include "tof/vl53l1x.h"
#include <stdint.h>
void tof_loop_iteration(vl53l1x_t *dev, SemaphoreHandle_t mutex);
int tof_main(void);
int tof_init(vl53l1x_t *dev);
#endif
