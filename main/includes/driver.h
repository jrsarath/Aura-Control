#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_matter.h>

using namespace esp_matter;

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

typedef void *driver_handle;

/** 
 * Initialize the input Switch driver
 * This initializes the input switch driver associated with the selected board.
 *
 * @return Handle on success.
 * @return NULL in case of failure.
 */
driver_handle input_switch_init(int gpio_pin, uint16_t endpoint_id);

/** 
 * Initialize the Switch driver
 * This initializes the switch driver associated with the selected board.
 *
 * @return Handle on success.
 * @return NULL in case of failure.
 */
driver_handle switch_init(int gpio_pin);

/** 
 * Initialize the button driver
 * This initializes the button driver associated with the selected board.
 *
 * @return Handle on success.
 * @return NULL in case of failure.
 */
driver_handle driver_button_init();

/** 
 * Driver Update
 * This API should be called to update the driver for the attribute being updated.
 * This is usually called from the common `app_attribute_update_cb()`.
 *
 * @param[in] endpoint_id Endpoint ID of the attribute.
 * @param[in] cluster_id Cluster ID of the attribute.
 * @param[in] attribute_id Attribute ID of the attribute.
 * @param[in] val Pointer to `esp_matter_attr_val_t`. Use appropriate elements as per the value type.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t driver_attribute_update(driver_handle driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

/**
 * Creates a switch
 * @return plug_unit_endpoint
 */
plug_unit_endpoint create_plug(int gpio_pin, node_t* node);

/**
 * Sensort attribute update callback
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t sensor_attribute_update_cb(esp_matter::attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data);

/**
 * Finds input pin by output pin
 * @return int
 */
int find_input_pin_by_output_pin(int outputPin);

/**
 * Input button callback
 * @return void
 */
// void driver_input_button_toggle_cb(void *arg, void *data);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()                                           \
    {                                                                                   \
        .radio_mode = RADIO_MODE_NATIVE,                                                \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()                                            \
    {                                                                                   \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,                              \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                            \
    {                                                                                   \
        .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10, \
    }
#endif

#ifdef __cplusplus
}
#endif