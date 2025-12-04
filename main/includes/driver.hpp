#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_matter.h>
#include <driver/gpio.h>
#include "variables.hpp"

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "esp_openthread_types.h"
#endif

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

typedef struct {
    int gpio_pin;
    uint16_t endpoint_id;
} gpio_isr_data_t;
struct gpio_plug {
    gpio_num_t GPIO_PIN_VALUE;
};
struct plug {
    gpio_num_t output_gpio_pin;
    gpio_num_t input_gpio_pin;
};
struct plug_unit_endpoint {
    uint16_t endpoint_id;
    gpio_num_t output_gpio_pin;
    gpio_num_t input_gpio_pin;
};
typedef void *driver_handle;

/**
 * @brief Start a non-blocking identification pulse on the physical switch.
 *
 */
void driver_identify_pulse(uint16_t endpoint_id);

/**
 * @brief Stop any running identification pulse.
 * 
 */
void driver_identify_stop(void);

/**
 * @brief Update the attribute value
 * 
 * @param driver_handle driver_handle
 * @param endpoint_id endpoint_id
 * @param cluster_id cluster_id
 * @param attribute_id attribute_id
 * @param val pointer to the attribute value
 * @return esp_err_t 
 */
esp_err_t driver_attribute_update(driver_handle driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

/**
 * @brief Initialize the driver
 * 
 * @return esp_err_t 
 */
esp_err_t driver_init(void);

/**
 * @brief Create a plug object
 * 
 * @param plug 
 * @param node 
 * @return esp_err_t 
 */
esp_err_t create_plug(plug* plug, esp_matter::node_t* node);

/**
 * @brief Initialize a plug object
 * 
 * @param plug plug pointer
 * @return esp_err_t 
 */
esp_err_t plug_init(plug* plug);

/**
 * @brief Initialize the input switch
 * 
 * @param gpio_pin GPIO pin number
 * @param endpoint_id Endpoint ID
 * @return driver_handle 
 */
driver_handle input_switch_init(int gpio_pin, uint16_t endpoint_id);

/**
 * @brief Initialize the button driver
 * 
 * @return driver_handle 
 */
driver_handle driver_button_init(void);