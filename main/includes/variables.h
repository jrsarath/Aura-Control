#pragma once

#include <inttypes.h>
#include <device.h>

#define VARIABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CONFIGURABLE_PLUGS 8
#define DEFAULT_POWER false
#define DEBOUNCE_DELAY_MS 1000

struct plug_unit_endpoint {
    uint16_t endpoint_id;
    int gpio_pin;
};

extern const int outputPins[MAX_CONFIGURABLE_PLUGS];
extern const int inputPins[MAX_CONFIGURABLE_PLUGS];

#ifdef __cplusplus
}
#endif