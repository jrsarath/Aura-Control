#pragma once


#include <inttypes.h>
#include "sdkconfig.h"
#include <driver/gpio.h>
#include "driver.hpp"

#define VARIABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CONFIGURABLE_PLUGS 8
#define DEFAULT_POWER false
#define DEBOUNCE_DELAY_MS 1000

extern const plug plugs[MAX_CONFIGURABLE_PLUGS];

#ifdef __cplusplus
}
#endif