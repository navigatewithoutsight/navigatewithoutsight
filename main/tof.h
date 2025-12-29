#ifndef vl53l7cx_H
#define vl53l7cx_H
#include "vl53l1x.h"
void tof_loop_iteration(vl53l1x_t*dev);
int tof_main(void);
int tof_init(vl53l1x_t *dev);
#endif

