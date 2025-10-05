#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <device.h>
#include <inttypes.h>
#include <iot_button.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "includes/variables.h"
#include "includes/driver.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "driver";
static uint16_t configured_plugs = 0;
static plug_unit_endpoint plug_unit_list[MAX_CONFIGURABLE_PLUGS];
static SemaphoreHandle_t plug_mutex = NULL;

int find_input_pin_by_output_pin(int outputPin) {
    for (int i = 0; i < sizeof(outputPins) / sizeof(outputPins[0]); ++i) {
        if (outputPins[i] == outputPin) {
            return inputPins[i];
        }
    }
    return -1;
}
static int get_gpio_index_by_endpoint(uint16_t endpoint_id) {
    for(int i = 0; i < configured_plugs; i++) {
        if (plug_unit_list[i].endpoint_id == endpoint_id) {
            return i;
        }
    }
    return -1;
}
static void driver_button_toggle_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Toggle button pressed");
}
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


esp_err_t driver_attribute_update(driver_handle driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
    esp_err_t err = ESP_OK;
    if (cluster_id == OnOff::Id) {
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
           int gpio_index = get_gpio_index_by_endpoint(endpoint_id);
           if (gpio_index != -1){
                int GPIO_PIN = plug_unit_list[gpio_index].gpio_pin;
                ESP_LOGI(TAG, "Toggling GPIO: %d, Val : %d", GPIO_PIN, val->val.b);
                gpio_set_level((gpio_num_t)GPIO_PIN, !val->val.b);
           } 
        }
    }
    return err;
}
esp_err_t driver_plug_unit_set_defaults(uint16_t endpoint_id, int gpio_pin) {
    esp_err_t err = ESP_OK;
    if ((gpio_num_t)gpio_pin != GPIO_NUM_NC){
        node_t *node = node::get();
        endpoint_t *endpoint = endpoint::get(node, endpoint_id);
        cluster_t *cluster = cluster::get(endpoint, OnOff::Id);
        attribute_t *attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);

        attribute::set_deferred_persistence(attribute);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);

        err |= gpio_set_level((gpio_num_t)gpio_pin, !val.val.b);
    } 

    return err;
}

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

esp_err_t driver_deinit(void) {
    if (plug_mutex) {
        vSemaphoreDelete(plug_mutex);
        plug_mutex = NULL;
    }
    return ESP_OK;
}

static esp_err_t add_plug_to_list(plug_unit_endpoint* plug) {
    esp_err_t ret = ESP_ERR_NO_MEM;
    
    if (xSemaphoreTake(plug_mutex, portMAX_DELAY) == pdTRUE) {
        if (configured_plugs < MAX_CONFIGURABLE_PLUGS) {
            memcpy(&plug_unit_list[configured_plugs], plug, sizeof(plug_unit_endpoint));
            configured_plugs++;
            ret = ESP_OK;
        }
        xSemaphoreGive(plug_mutex);
    }
    return ret;
}

esp_err_t delete_plug(uint16_t endpoint_id) {
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    
    if (xSemaphoreTake(plug_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < configured_plugs; i++) {
            if (plug_unit_list[i].endpoint_id == endpoint_id) {
                // Shift remaining elements
                for (int j = i; j < configured_plugs - 1; j++) {
                    memcpy(&plug_unit_list[j], &plug_unit_list[j + 1], sizeof(plug_unit_endpoint));
                }
                configured_plugs--;
                ret = ESP_OK;
                break;
            }
        }
        xSemaphoreGive(plug_mutex);
    }
    return ret;
}

esp_err_t get_plug_state(uint16_t endpoint_id, bool* state) {
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    
    if (xSemaphoreTake(plug_mutex, portMAX_DELAY) == pdTRUE) {
        int gpio_index = get_gpio_index_by_endpoint(endpoint_id);
        if (gpio_index != -1) {
            int gpio_pin = plug_unit_list[gpio_index].gpio_pin;
            *state = gpio_get_level((gpio_num_t)gpio_pin);
            ret = ESP_OK;
        }
        xSemaphoreGive(plug_mutex);
    }
    return ret;
}

plug_unit_endpoint create_plug(int gpio_pin, node_t* node) {
    plug_unit_endpoint switch_details = {-1, -1}; // Initialize with invalid values
    
    if (plug_mutex == NULL) {
        ESP_LOGE(TAG, "Driver not initialized");
        return switch_details;
    }

    driver_handle handle = switch_init(gpio_pin);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to initialize switch");
        return switch_details;
    }

    on_off_plugin_unit::config_t config;
    config.on_off.on_off = DEFAULT_POWER;
    config.on_off.lighting.start_up_on_off = nullptr;
    
    endpoint_t *endpoint = on_off_plugin_unit::create(node, &config, ENDPOINT_FLAG_NONE, handle);
    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create switch endpoint");
        return switch_details;
    }

    switch_details.gpio_pin = gpio_pin;
    switch_details.endpoint_id = endpoint::get_id(endpoint);

    if (add_plug_to_list(&switch_details) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add plug to list");
        // TODO: Clean up endpoint
        return {-1, -1};
    }

    driver_plug_unit_set_defaults(switch_details.endpoint_id, gpio_pin);
    
    // Create fixed label cluster
    cluster::fixed_label::config_t fl_config;
    cluster::fixed_label::create(endpoint, &fl_config, CLUSTER_FLAG_SERVER);

    ESP_LOGI(TAG, "Plug created with endpoint_id %d", switch_details.endpoint_id);
    return switch_details;
}

driver_handle switch_init(int gpio_pin) {
    if (gpio_pin < 0 || gpio_pin >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid GPIO pin number: %d", gpio_pin);
        return nullptr;
    }

    button_config_t config = button_driver_get_config();
    config.gpio_button_config.gpio_num = gpio_pin;
    button_handle_t handle = iot_button_create(&config);
    
    if (handle == nullptr) {
        ESP_LOGE(TAG, "Failed to create button handle for GPIO: %d", gpio_pin);
        return nullptr;
    }

    esp_err_t err = gpio_set_direction((gpio_num_t)gpio_pin, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO direction: %d", err);
        iot_button_delete(handle);
        return nullptr;
    }

    err = gpio_set_pull_mode((gpio_num_t)gpio_pin, GPIO_PULLUP_ONLY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO pull mode: %d", err);
        iot_button_delete(handle);
        return nullptr;
    }

    return (driver_handle)handle;
}
driver_handle input_switch_init(int gpio_pin, uint16_t endpoint_id) {
    if (gpio_pin < 0 || gpio_pin >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid GPIO pin number: %d", gpio_pin);
        return nullptr;
    }

    button_config_t config = button_driver_get_config();
    config.gpio_button_config.gpio_num = gpio_pin;
    button_handle_t handle = iot_button_create(&config);

    if (handle == nullptr) {
        ESP_LOGE(TAG, "Failed to create button handle for GPIO: %d", gpio_pin);
        return nullptr;
    }

    plug_unit_endpoint* callback_data = (plug_unit_endpoint*)calloc(1, sizeof(plug_unit_endpoint));
    if (callback_data == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for callback data");
        iot_button_delete(handle);
        return nullptr;
    }

    callback_data->endpoint_id = endpoint_id;
    callback_data->gpio_pin = gpio_pin;
    ESP_LOGI(TAG, "endpoint_id %d", (int)endpoint_id);

    esp_err_t err = iot_button_register_cb(handle, BUTTON_PRESS_DOWN, driver_input_button_toggle_cb, callback_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register button callback: %d", err);
        free(callback_data);
        iot_button_delete(handle);
        return nullptr;
    }

    err = gpio_set_direction((gpio_num_t)gpio_pin, GPIO_MODE_INPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO direction: %d", err);
        free(callback_data);
        iot_button_delete(handle);
        return nullptr;
    }

    err = gpio_set_pull_mode((gpio_num_t)gpio_pin, GPIO_PULLUP_ONLY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO pull mode: %d", err);
        free(callback_data);
        iot_button_delete(handle);
        return nullptr;
    }

    return (driver_handle)handle;
}
driver_handle driver_button_init() {
    button_config_t config = button_driver_get_config();
    button_handle_t handle = iot_button_create(&config);
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, driver_button_toggle_cb, NULL);
    return (driver_handle)handle;
}

void device_identifier_cb() {
    gpio_set_direction((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, GPIO_PULLUP_ONLY);

    for (int blink_count = 0; blink_count < 6; blink_count++) {
        gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 0);
}
void device_commission_window_open_cb() {
    gpio_set_direction((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, GPIO_PULLUP_ONLY);

    while (1) {
        gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 1);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 0);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}
void device_commission_window_close_cb() {
    gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 0);
}