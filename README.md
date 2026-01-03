# README

## Description

This project contains software for ESP32-based device to "navigate without sight".

## Quick start

### Prerequisites

1. **Clone the repo:** `git clone git@github.com:navigatewithoutsight/navigatewithoutsight.git`
2. **Install & set up ESP-IDF.** If you haven't already, follow the [official getting started guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).
    - *Important:* Check the recommended [ESP-IDF version](#version) below.
3. **Activate the IDF environment** in your terminal (e.g., run `source $IDF_PATH/export.sh` or use the ESP-IDF terminal).

### First Build & Flash

1. Navigate to the project root.
2. Set the target (once): `idf.py set-target esp32`
3. Build, flash, and monitor (make sure your device is connected):

    ```bash
    idf.py build flash monitor
    ```

    > **Note:** If flashing fails, check the serial port with `idf.py flash -p PORT` (e.g., `COM3` on Windows, `/dev/ttyUSB0` on Linux/Mac).
4. Once the device runs, read the [Project Rules](#project-rules) and learn how to [add your code](#how-to-add-your-code).

## Building

### Version

Before start your work make sure you use the same idf.py version as other members.

Go to shell(link for other ways to check version can be found in [official guide](<https://docs.espressif.com/projects/esp-idf/en/stable/esp32/versions.html#checking->
the-current-version)):

```bash
  idf.py --version
```

Example output: ESP-IDF v5.2.6

Currently there are no agreed version. Project probably builds and runs under all versions within 5.2.x - 5.3.x.

### Build & run

Standard set of commands:

1. Prepare environment(idf.py must be visible): `which idf.py`(`where` on Windows)
2. Set target: `idf.py set-target esp32`
3. Build: `idf.py build`
4. Flash: `idf.py flash`
5. Monitor: `idf.py monitor`

[!hint] Commands can be inlined: `idf.py clean menuconfig build flash monitor`

If some step broke check `clean` or `fullclean`, might help

## Project rules

### Git

#### Commit Philosophy

Each commit should represent a single logical change (a fix, refactor, or part of a feature). Use descriptive commit messages.
> Think of a commit as a snapshot of your progress. Small, focused commits are encouraged. It's common to push a series of 5-6 commits at once that together implement one feature.

#### What to Track (and What Not To)

- **Do NOT commit** temporary files, API keys, passwords, or personal IDE settings. Use the `.gitignore` file.
- **Do NOT commit** your local `sdkconfig` file. The `sdkconfig.defaults` is for project-wide defaults.

#### Branching Strategy (Proposed)

To keep the main branch stable, we suggest a simple feature-branch workflow:

1. Create a new branch for your task: `git checkout -b feat/your-feature-name`
2. Commit your changes to that branch.
3. Push the branch and open a **Pull Request (PR)** for review.
4. After approval, the PR is merged into `main`.
This ensures code review and prevents broken builds on the main branch.

### Code

Due to project structure and for sake of testability, mockability and integration, separate actual implementation and calls.
Generally it's about keeping your interfaces clean.
Example:

```c

// Bad: there are no reuse
void app_main() {
  // Initialization
  init_step_1(...);
  init_step_2(...);
  init_step_3(...);

  while (1) {
    sensor_read(...);
    process(...);
    output(...);
  }
  return;
}

// We would need to call separate functions:
void all_modules_main() {
  // Other modules
  gy87_init();
  tof_init();
  buffon_init();

  // Your separate functions
  init_step_1(...);
  init_step_2(...);
  init_step_3(...);

  while (1) {
    gy87_iteration(...);
    tof_iteration(...);

    ...
    sensor_read(...); // This is a mess
    process(...);
    output(...);
  }
}
```

On other hand:

```c
// This function can be called from app_main, tests, or other modules.
void sensor_init(void) {
  init_step_1(...);
  init_step_2(...);
  init_step_3(...);
}

void sensor_iteration(void) {
  sensor_read(...);
  process(...);
  output(...);
}

// Your main can do:
void app_main() {
    sensor_init();
    while (1) {
      sensor_iteration()
  }
}

// And from other parts
// We would need to call separate functions:
void all_modules_main() {
  // Other modules
  gy87_init();
  tof_init();
  buffon_init();

  // Your dedicated function
  sensor_init();

  while (1) {
    gy87_iteration(...);
    tof_iteration(...);
    sensor_iteration(...); // Nice and clean
  }
}

```

## Codebase Structure

The repository has the following structure (as of commit `08bb8dd`):

```tree
.
├── main/                     # Our application source code
│   ├── CMakeLists.txt        # CMake build instructions for the main app
│   ├── gy87.c                # GY87 (IMU) sensor implementation
│   ├── gy87.h                # GY87 header file
│   ├── idf_component.yml     # Manages third-party dependencies
│   ├── Kconfig.projbuild     # Aggregates project-specific Kconfig menus
│   ├── main.c                # Application entry point (selects module via config)
│   ├── real_main.c           # Main function for the integrated application (MODULE_ALL)
│   ├── real_main.h           # Header for the integrated application
│   ├── tof/                  # Time-of-Flight (ToF) sensor module
│   │   ├── Kconfig           # ToF-specific configuration options
│   │   └── tof.c             # ToF sensor implementation
│   └── tof.h                 # ToF header file
├── managed_components/       # Third-party dependencies (managed by IDF)
│   └── vl53l1x_dep           # ToF sensor driver dependency
├── README.md                 # This file
├── build/                    # Build directory (generated, do not commit)
├── CMakeLists.txt            # Top-level CMake file
├── dependencies.lock         # Locks dependency versions for reproducibility
├── sdkconfig                 # Your local build configuration (do not commit)
├── sdkconfig.defaults        # Project-wide default configuration
└── sdkconfig.old             # Previous configuration backup
```

This structure is bad tho, try please to add your component via `idf.py create-component <name>` instead. This structure will be fixed if we have time.

## Config

### Gen approach info

Idf has it's way of configuring project. If you type:

```bash
idf.py menuconfig
```

It will open a configuration pane. On save it configures `sdkconfig` to be used in build.

It looks smth like this:

```md
                                                                         Espressif IoT Development Framework Configuration
    Build type  --->
    Bootloader config  --->
    Security features  --->
    Application manager  --->
    Serial flasher config  --->
    Partition Table  --->
    Module Configuration  --->
    Compiler options  --->
    Component config  --->
[ ] Make experimental features visible
```

### What we came up with

Most of it is build-in, although for our project we added custom variables. They can be found in:

```md
    Module Configuration  --->        # This is for configuring our sensors
```

There you can find a build to run:

```md
(Top) → Module Configuration → Select module to run
                                                                         Espressif IoT Development Framework Configuration
( ) GY87 Module (IMU)
( ) Enable VL53L7CX ToF sensor
        VL53L7CX Configuration  --->
(x) All Modules (Integration)
```

### Build types

So far there are few options:

- GY87 Module (IMU)

Uses GY87's main function and designed to test functionality only of this sensor

- ToF sensor

Uses TOF main function and designed to test functionality only of this sensor

- All Modules

Used to build whole project. So far there are no functionality to disable separate modules in integration with mocks, yet it might be useful.

### How to add your code?

1. Add mention of your module to config(Kconfig + main/Kconfig.projbuild)
2. Add your code under main/
3. Add your `app_main()`(better change name tho to `_your_module_main()`) and `#include` under \#ifdef/\#endif directives in main/main.c
4. Run `idf.py menuconfig` to select your build. Check it and save.
5. Build and run
