#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <device.h>
#include <inttypes.h>
#include <iot_button.h>
#include <driver/gpio.h>

#include "includes/variables.h"
#include "includes/driver.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "driver";
static uint16_t configured_plugs = 0;
static plug_unit_endpoint plug_unit_list[MAX_CONFIGURABLE_PLUGS];

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
plug_unit_endpoint create_plug(int gpio_pin, node_t* node) {
    plug_unit_endpoint switch_details;
    driver_handle handle = switch_init(gpio_pin);
    on_off_plugin_unit::config_t config;
    config.on_off.on_off = DEFAULT_POWER;
    config.on_off.lighting.start_up_on_off = nullptr;
    endpoint_t *endpoint = on_off_plugin_unit::create(node, &config, ENDPOINT_FLAG_NONE, handle);
    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create switch endpoint for %d", gpio_pin);
        switch_details.endpoint_id = -1;
        switch_details.gpio_pin = -1;
        return switch_details;
    }

    for (int i = 0; i < configured_plugs; i++) {
        if (plug_unit_list[i].gpio_pin == gpio_pin) {
            ESP_LOGI(TAG, "Switch already configured: %d", endpoint::get_id(endpoint));
            switch_details.endpoint_id = endpoint::get_id(endpoint);
            switch_details.gpio_pin = gpio_pin;
            return switch_details;
        }
    }

    if (configured_plugs < MAX_CONFIGURABLE_PLUGS) {
        plug_unit_list[configured_plugs].gpio_pin = gpio_pin;
        plug_unit_list[configured_plugs].endpoint_id = endpoint::get_id(endpoint);
        driver_plug_unit_set_defaults(endpoint::get_id(endpoint), gpio_pin);
        configured_plugs++;
    } else {
        ESP_LOGI(TAG, "Cannot configure more plugs");
        switch_details.endpoint_id = -1;
        switch_details.gpio_pin = -1;
        return switch_details;
    }


    static uint16_t plug_endpoint_id = 0;
    plug_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Plug created with endpoint_id %d", plug_endpoint_id);

    cluster::fixed_label::config_t fl_config;
    cluster::fixed_label::create(endpoint, &fl_config, CLUSTER_FLAG_SERVER);

    switch_details.endpoint_id = plug_endpoint_id;
    switch_details.gpio_pin = gpio_pin;
    return switch_details;
}

driver_handle switch_init(int gpio_pin) {
    button_config_t config = button_driver_get_config();
    config.gpio_button_config.gpio_num = gpio_pin;
    button_handle_t handle = iot_button_create(&config);

    gpio_set_direction((gpio_num_t)gpio_pin, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)gpio_pin, GPIO_PULLUP_ONLY);
    return (driver_handle)handle;
}
driver_handle input_switch_init(int gpio_pin, uint16_t endpoint_id) {

    button_config_t config = button_driver_get_config();
    config.gpio_button_config.gpio_num = gpio_pin;
    button_handle_t handle = iot_button_create(&config);

    plug_unit_endpoint* callback_data = (plug_unit_endpoint*)malloc(sizeof(plug_unit_endpoint));
    callback_data->endpoint_id = endpoint_id;
    callback_data->gpio_pin = gpio_pin;
    ESP_LOGI(TAG, "endpoint_id %d", (int)endpoint_id);
    iot_button_register_cb((button_handle_t)handle, BUTTON_PRESS_DOWN, driver_input_button_toggle_cb, callback_data);

    gpio_set_direction((gpio_num_t)gpio_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)gpio_pin, GPIO_PULLUP_ONLY);

    return (driver_handle)handle;
};
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