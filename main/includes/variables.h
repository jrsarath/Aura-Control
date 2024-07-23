#pragma once

#include <inttypes.h>
#include <device.h>

#define VARIABLES_H

#ifdef __cplusplus
extern "C" {
#endif

struct plug_unit_endpoint {
    uint16_t endpoint_id;
    int gpio_pin;
};

#define MAX_CONFIGURABLE_PLUGS 8
#define DEFAULT_POWER false

#ifdef __cplusplus
}
#endif