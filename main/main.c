#include "sdkconfig.h"
#include "stdio.h"
#if CONFIG_VL53L7CX_ENABLE
#include "tof.h"
#endif
#if CONFIG_MODULE_GY87
#include "gy87.h"
#endif
#if CONFIG_MODULE_ALL
#include "real_main.h"
#elif CONFIG_IO_TEST
#include "io_test.h"
#endif

void app_main() {
  printf("Build configuration:\n");

#if CONFIG_MODULE_GY87
  printf("- GY87 module enabled\n");
  gy87_main();
#elif CONFIG_VL53L7CX_ENABLE
  printf("- TOF module enabled\n");
  tof_main();
#elif CONFIG_MODULE_ALL
  printf("- All modules enabled\n");
  all_modules_main();
#elif CONFIG_IO_TEST
  printf("- IO_TEST\n");
  io_test_start();
#endif

#if CONFIG_DEBUG_OUTPUT
  printf("Debug output enabled\n");
#endif
}
