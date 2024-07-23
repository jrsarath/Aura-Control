#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <device.h>
#include <driver/gpio.h>

#include "includes/variables.h"
#include "includes/driver.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "driver";

static void driver_button_toggle_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Toggle button pressed");
}

esp_err_t create_plug(gpio_num_t gpio_pin, node_t* node) {
    esp_err_t err = ESP_OK;
    driver_handle handle = switch_init(gpio_pin);
    on_off_plugin_unit::config_t config;
    config.on_off.on_off = DEFAULT_POWER;
    config.on_off.lighting.start_up_on_off = nullptr;
    endpoint_t *endpoint = on_off_plugin_unit::create(node, &config, ENDPOINT_FLAG_NONE, handle);
    ESP_LOGE(TAG, "Failed to create switch endpoint for %d", gpio_pin);

    static uint16_t plug_endpoint_id = 0;
    plug_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Plug created with endpoint_id %d", plug_endpoint_id);

    cluster::fixed_label::config_t fl_config;
    cluster::fixed_label::create(endpoint, &fl_config, CLUSTER_FLAG_SERVER);
    return err;
}
esp_err_t driver_attribute_update(driver_handle driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
    esp_err_t err = ESP_OK;
    
    if (cluster_id == OnOff::Id) {
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
            gpio_set_level(GPIO_NUM_13, !val->val.b);
        //    int gpio_index = get_gpio_index(endpoint_id);
        //    if (gpio_index != -1){
        //         ESP_LOGI(TAG, "Toggling GPIO: %d, Val : %d", gpio.at(gpio_index), val->val.b);
        //         gpio_set_level(gpio.at(gpio_index), !val->val.b);
        //    } 
        }
    }
    return err;
}

driver_handle switch_init(gpio_num_t gpio_pin) {
    button_config_t config = button_driver_get_config();
    config.gpio_button_config.gpio_num = gpio_pin;
    button_handle_t handle = iot_button_create(&config);

    gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(gpio_pin, GPIO_PULLUP_ONLY);
    return (driver_handle)handle;
}
driver_handle driver_button_init() {
    button_config_t config = button_driver_get_config();
    button_handle_t handle = iot_button_create(&config);
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, driver_button_toggle_cb, NULL);
    return (driver_handle)handle;
}