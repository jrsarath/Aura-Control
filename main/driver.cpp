#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include "bsp/esp-bsp.h"
#include <esp_matter.h>
#include <inttypes.h>
#include <driver/gpio.h>
#include <button_gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "includes/variables.hpp"
#include "includes/driver.hpp"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "driver";
static uint16_t configured_plugs = 0;
static plug_unit_endpoint plug_unit_list[MAX_CONFIGURABLE_PLUGS];
static SemaphoreHandle_t plug_mutex = NULL;
// Identification pulse state (driver-side)
static TaskHandle_t s_ident_task_drv = NULL;
static volatile bool s_ident_running_drv = false;
static uint16_t s_ident_count_drv = 0;
static gpio_num_t s_ident_gpio_drv = GPIO_NUM_NC;

/**
 * @brief Get the gpio by endpoint object
 * 
 * @param endpoint_id 
 * @return gpio_num_t 
 */
static gpio_num_t get_gpio_by_endpoint(uint16_t endpoint_id) {
    gpio_num_t gpio_pin = GPIO_NUM_NC;
    for (int i = 0; i < configured_plugs; i++) {
        if (plug_unit_list[i].endpoint_id == endpoint_id) {
            gpio_pin = plug_unit_list[i].output_gpio_pin;
        }
    }
    return gpio_pin;
}

/**
 * @brief Update the GPIO value
 * 
 * @param pin gpio pin number
 * @param value new value to set
 * @return esp_err_t 
 */
static esp_err_t driver_update_gpio_value(gpio_num_t pin, bool value) {
    esp_err_t err = ESP_OK;

    ESP_LOGI(TAG, "Setting GPIO pin : %d to %d", pin, value);
    err = gpio_set_level(pin, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO level");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "GPIO pin : %d set to %d", pin, value);
    }
    return err;
}

/**
 * @brief Identification task for driver
 * 
 * @param arg 
 */
static void identification_task_drv(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Driver identification task started (blinks=%u)", s_ident_count_drv);

    const uint32_t on_ms = 2000;

    int orig_level = -1;
    if (s_ident_gpio_drv != GPIO_NUM_NC) {
        // ensure gpio is output so we can set level
        gpio_set_direction(s_ident_gpio_drv, GPIO_MODE_OUTPUT);
        orig_level = gpio_get_level(s_ident_gpio_drv);
    }

    // Single simple toggle: set opposite, wait 2s, restore original
    if (s_ident_gpio_drv != GPIO_NUM_NC && orig_level >= 0) {
        ESP_LOGI(TAG, "Driver identification task, Setting GPIO %d to %d", s_ident_gpio_drv, !orig_level);
        gpio_set_level(s_ident_gpio_drv, !orig_level);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        ESP_LOGI(TAG, "Driver identification task, Returning GPIO %d to %d", s_ident_gpio_drv, orig_level);
        gpio_set_level(s_ident_gpio_drv, orig_level);
    }

    ESP_LOGI(TAG, "Driver identification task stopping");
    s_ident_running_drv = false;
    TaskHandle_t t = s_ident_task_drv;
    s_ident_task_drv = NULL;
    if (t) vTaskDelete(NULL);
}

/**
 * @brief Input button callback
 * 
 * @param arg 
 * @param data 
 */
static void driver_button_toggle_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Toggle button pressed");
}

/**
 * @brief Input button callback
 * 
 * @param arg 
 * @param data 
 */
static void driver_input_button_toggle_cb(void *arg, void *data) {
    plug_unit_endpoint* callback_data = (plug_unit_endpoint*) data;
    ESP_LOGI(TAG, "Toggle button pressed, %d", callback_data->endpoint_id);

    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, callback_data->endpoint_id);
    cluster_t *cluster = cluster::get(endpoint, OnOff::Id);
    attribute_t *attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(callback_data->endpoint_id, cluster::get_id(cluster), attribute::get_id(attribute), &val);
}

/**
 * @brief Start the driver identification pulse
 * 
 * @param endpoint_id 
 */
void driver_identify_pulse(uint16_t endpoint_id) {
    // cancel previous
    if (s_ident_running_drv) {
        driver_identify_stop();
    }

    // For switches, a single on/off (or off/on) pulse is sufficient
    uint32_t blinks = 1;

    gpio_num_t gpio = get_gpio_by_endpoint(endpoint_id);
    if (gpio == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "No GPIO mapping for endpoint %d", endpoint_id);
        return;
    }

    s_ident_gpio_drv = gpio;
    s_ident_count_drv = blinks;
    s_ident_running_drv = true;

    BaseType_t created = xTaskCreate(identification_task_drv, "drv_ident", 3072, NULL, tskIDLE_PRIORITY + 1, &s_ident_task_drv);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create driver identification task");
        s_ident_running_drv = false;
        s_ident_task_drv = NULL;
    }
}

/**
 * @brief Stop the driver identification pulse
 * 
 */
void driver_identify_stop(void) {
    if (!s_ident_running_drv && s_ident_task_drv == NULL) return;
    s_ident_running_drv = false;
    // Wait long enough for the single toggle to finish (on_ms ~= 2000ms)
    const TickType_t wait_ticks = pdMS_TO_TICKS(3000);
    const TickType_t poll_ticks = pdMS_TO_TICKS(50);
    TickType_t waited = 0;
    while (s_ident_task_drv != NULL && waited < wait_ticks) {
        vTaskDelay(poll_ticks);
        waited += poll_ticks;
    }
    if (s_ident_task_drv != NULL) {
        vTaskDelete(s_ident_task_drv);
        s_ident_task_drv = NULL;
    }
    s_ident_running_drv = false;
    s_ident_gpio_drv = GPIO_NUM_NC;
}

/**
 * @brief Update the attribute value
 * 
 * @param driver_handle driver_handle
 * @param endpoint_id endpoint_id
 * @param cluster_id cluster_id
 * @param attribute_id attribute_id
 * @param val val
 * @return esp_err_t
 */
esp_err_t driver_attribute_update(driver_handle driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "Driver attribute update called for endpoint_id: %d, cluster_id: %" PRIu32 ", attribute_id: %" PRIu32 ", value: %d", endpoint_id, cluster_id, attribute_id, val->val.b);

    if (cluster_id == OnOff::Id) {
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
            gpio_num_t gpio_pin = get_gpio_by_endpoint(endpoint_id);
            if (gpio_pin != GPIO_NUM_NC) {
                err = driver_update_gpio_value(gpio_pin, !val->val.b);
            } else {
                ESP_LOGE(TAG, "GPIO pin mapping for endpoint_id: %d not found", endpoint_id);
                return ESP_FAIL;
            }
        }
    }
    return err;
}

/**
 * @brief Set the default state of the plug unit
 * 
 * @param endpoint_id 
 * @param gpio_pin 
 * @return esp_err_t 
 */
esp_err_t driver_plug_unit_set_defaults(uint16_t endpoint_id, gpio_num_t gpio_pin) {
    esp_err_t err = ESP_OK;
    
    if (gpio_pin != GPIO_NUM_NC){
        node_t *node = node::get();
        endpoint_t *endpoint = endpoint::get(node, endpoint_id);
        cluster_t *cluster = cluster::get(endpoint, OnOff::Id);
        attribute_t *attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);

        attribute::set_deferred_persistence(attribute);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);

        ESP_LOGI(TAG, "Setting default state for endpoint_id: %d, gpio_pin: %d, state: %d", endpoint_id, gpio_pin, !val.val.b);
        err |= driver_update_gpio_value(gpio_pin, !val.val.b);
    } 

    return err;
}

/**
 * @brief Initialize the driver
 * 
 * @return esp_err_t 
 */
esp_err_t driver_init(void) {
    if (plug_mutex == NULL) {
        plug_mutex = xSemaphoreCreateMutex();
        if (plug_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }
    configured_plugs = 0;
    memset(plug_unit_list, 0, sizeof(plug_unit_list));
    return ESP_OK;
}

// esp_err_t get_plug_state(uint16_t endpoint_id, bool* state) {
//     esp_err_t ret = ESP_ERR_NOT_FOUND;
    
//     if (xSemaphoreTake(plug_mutex, portMAX_DELAY) == pdTRUE) {
//         int gpio_index = get_gpio_index_by_endpoint(endpoint_id);
//         if (gpio_index != -1) {
//             int gpio_pin = plug_unit_list[gpio_index].gpio_pin;
//             *state = gpio_get_level((gpio_num_t)gpio_pin);
//             ret = ESP_OK;
//         }
//         xSemaphoreGive(plug_mutex);
//     }
//     return ret;
// }

/**
 * @brief Create a plug unit and associate it with a Matter node.
 * 
 * @param plug Pointer to the plug structure containing GPIO pin information.
 * @param node Pointer to the Matter node.
 */
esp_err_t create_plug(plug* plug, node_t* node) {
    esp_err_t err = ESP_OK;

    if (!node) {
        ESP_LOGE(TAG, "Matter node cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!plug) {
        ESP_LOGE(TAG, "Plug cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Check for plug if already configured.
    for (int i = 0; i < configured_plugs; i++) {
        if (plug_unit_list[i].output_gpio_pin == plug->output_gpio_pin) {
            ESP_LOGE(TAG, "Plug already configured for gpio pin : %d", plug->output_gpio_pin);
            return ESP_ERR_INVALID_STATE;
        }
    }

    on_off_plugin_unit::config_t config;
    config.on_off.on_off = DEFAULT_POWER;
    endpoint_t *endpoint = on_off_plugin_unit::create(node, &config, ENDPOINT_FLAG_NONE, plug);
    if (!endpoint) {
        ESP_LOGE(TAG, "Matter endpoint creation failed");
        return ESP_FAIL;
    }

    // GPIO pin Initialization
    err = plug_init(plug);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize plug");
    }

    // Check for maximum plugs that can be configured.
    if (configured_plugs < MAX_CONFIGURABLE_PLUGS) {
        plug_unit_list[configured_plugs].output_gpio_pin = plug->output_gpio_pin;
        plug_unit_list[configured_plugs].endpoint_id = endpoint::get_id(endpoint);
        driver_plug_unit_set_defaults(endpoint::get_id(endpoint), plug->output_gpio_pin);
        configured_plugs++;
    } else {
        ESP_LOGE(TAG, "Maximum plugs configuration limit exceeded!!!");
        return ESP_FAIL;
    }

    uint16_t plug_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Plug created with endpoint_id %d", plug_endpoint_id);

    cluster::fixed_label::config_t fl_config;
    cluster::fixed_label::create(endpoint, &fl_config, CLUSTER_FLAG_SERVER);

    return err;
}

/**
 * @brief Initialize a plug on the specified GPIO pin.
 * 
 * @param plug Pointer to the plug structure containing GPIO pin information.
 * @return A driver handle for the initialized plug, or nullptr on failure.
 */
esp_err_t plug_init(plug* plug) {
    esp_err_t err = ESP_OK;

    gpio_reset_pin(plug->output_gpio_pin);

    err = gpio_set_direction(plug->output_gpio_pin, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to set GPIO OUTPUT mode");
        return ESP_FAIL;
    }

    err = driver_update_gpio_value(plug->output_gpio_pin, DEFAULT_POWER ? 0 : 1);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Unable to set GPIO level");
    }
    return err;
}

// driver_handle input_switch_init(int gpio_pin, uint16_t endpoint_id) {
//     if (gpio_pin < 0 || gpio_pin >= GPIO_NUM_MAX) {
//         ESP_LOGE(TAG, "Invalid GPIO pin number: %d", gpio_pin);
//         return nullptr;
//     }

//     button_config_t config = button_driver_get_config();
//     config.gpio_button_config.gpio_num = gpio_pin;
//     button_handle_t handle = iot_button_create(&config);

//     if (handle == nullptr) {
//         ESP_LOGE(TAG, "Failed to create button handle for GPIO: %d", gpio_pin);
//         return nullptr;
//     }

//     plug_unit_endpoint* callback_data = (plug_unit_endpoint*)calloc(1, sizeof(plug_unit_endpoint));
//     if (callback_data == nullptr) {
//         ESP_LOGE(TAG, "Failed to allocate memory for callback data");
//         iot_button_delete(handle);
//         return nullptr;
//     }

//     callback_data->endpoint_id = endpoint_id;
//     callback_data->gpio_pin = gpio_pin;
//     ESP_LOGI(TAG, "endpoint_id %d", (int)endpoint_id);

//     esp_err_t err = iot_button_register_cb(handle, BUTTON_PRESS_DOWN, driver_input_button_toggle_cb, callback_data);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to register button callback: %d", err);
//         free(callback_data);
//         iot_button_delete(handle);
//         return nullptr;
//     }

//     err = gpio_set_direction((gpio_num_t)gpio_pin, GPIO_MODE_INPUT);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to set GPIO direction: %d", err);
//         free(callback_data);
//         iot_button_delete(handle);
//         return nullptr;
//     }

//     err = gpio_set_pull_mode((gpio_num_t)gpio_pin, GPIO_PULLUP_ONLY);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to set GPIO pull mode: %d", err);
//         free(callback_data);
//         iot_button_delete(handle);
//         return nullptr;
//     }

//     return (driver_handle)handle;
// }

/**
 * @brief Initialize the driver button.
 * 
 * @return A driver handle for the initialized button.
 */
driver_handle driver_button_init() {
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));
    ESP_ERROR_CHECK(iot_button_register_cb(btns[0], BUTTON_PRESS_DOWN, NULL, driver_button_toggle_cb, NULL));
    
    return (driver_handle)btns[0];
}