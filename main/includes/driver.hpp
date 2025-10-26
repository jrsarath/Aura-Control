#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_matter.h>
#include "variables.hpp"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "esp_openthread_types.h"
#endif

#define DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

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
 * @brief Initialize a switch object
 * 
 * @param plug plug pointer
 * @return esp_err_t 
 */
esp_err_t switch_init(plug* plug);


esp_err_t delete_plug(uint16_t endpoint_id);
esp_err_t get_plug_state(uint16_t endpoint_id, bool* state);

// GPIO management

driver_handle input_switch_init(int gpio_pin, uint16_t endpoint_id);

/**
 * @brief Initialize the button driver
 * 
 * @return driver_handle 
 */
driver_handle driver_button_init(void);

// Callbacks
esp_err_t driver_attribute_update(driver_handle driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

/**
 * @brief Device identifier callback
 */
void device_identifier_cb(void);

/**
 * @brief Device commission window open callback
 */
void device_commission_window_open_cb(void);

/**
 * @brief Device commission window close callback
 */
void device_commission_window_close_cb(void);

esp_err_t sensor_attribute_update_cb(esp_matter::attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data);

int find_input_pin_by_output_pin(int outputPin);

/**
 * Input button callback
 * @return void
 */
// void driver_input_button_toggle_cb(void *arg, void *data);

#ifdef __cplusplus
}
#endif