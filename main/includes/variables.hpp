#pragma once


#include <inttypes.h>
#include "sdkconfig.h"
#include <driver/gpio.h>

#define VARIABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CONFIGURABLE_PLUGS 8
#define DEFAULT_POWER false
#define DEBOUNCE_DELAY_MS 1000

// Runtime plug data
struct plug_unit_endpoint {
    uint16_t endpoint_id;
    gpio_num_t output_gpio_pin;
    gpio_num_t input_gpio_pin;
};

extern const plug_unit_endpoint plugs[MAX_CONFIGURABLE_PLUGS];

#ifdef __cplusplus
}
#endif