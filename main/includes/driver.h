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

typedef struct {
    int gpio_pin;
    uint16_t endpoint_id;
} plug_unit_endpoint;

// State management
esp_err_t driver_init(void);
esp_err_t driver_deinit(void);

// Plug unit management
plug_unit_endpoint create_plug(int gpio_pin, esp_matter::node_t* node);
esp_err_t delete_plug(uint16_t endpoint_id);
esp_err_t get_plug_state(uint16_t endpoint_id, bool* state);

// GPIO management
driver_handle switch_init(int gpio_pin);
driver_handle input_switch_init(int gpio_pin, uint16_t endpoint_id);
driver_handle driver_button_init(void);

// Callbacks
esp_err_t driver_attribute_update(driver_handle driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);
void device_identifier_cb(void);
void device_commission_window_open_cb(void);
void device_commission_window_close_cb(void);

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