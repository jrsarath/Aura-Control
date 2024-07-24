#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <device.h>
#include <iot_button.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "includes/variables.h"
#include "includes/driver.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "driver";
static uint16_t configured_plugs = 0;
static plug_unit_endpoint plug_unit_list[MAX_CONFIGURABLE_PLUGS];
static bool button_was_pressed[MAX_CONFIGURABLE_PLUGS] = {false};
static bool button_is_pressed[MAX_CONFIGURABLE_PLUGS] = {false};
static QueueHandle_t gpio_evt_queue = NULL;

static int get_gpio_index_by_endpoint(uint16_t endpoint_id) {
    for(int i = 0; i < configured_plugs; i++) {
        if (plug_unit_list[i].endpoint_id == endpoint_id) {
            return i;
        }
    }
    return -1;
}
static int get_gpio_index_by_gpio_pin(int pin) {
    for(int i = 0; i < configured_plugs; i++) {
        if (plug_unit_list[i].gpio_pin == pin) {
            return i;
        }
    }
    return -1;
}
static void driver_button_toggle_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Toggle button pressed");
}
static void driver_input_button_toggle_cb(void *arg, void *data) {
    uint16_t* endpoint_id = (uint16_t*) data;
    ESP_LOGI(TAG, "Endpoint id %d", *endpoint_id);
    // int gpio_index = get_gpio_index_by_endpoint((int)endpoint_id);
    // if (gpio_index != -1) {
    //     int GPIO_PIN = plug_unit_list[gpio_index].gpio_pin;
    //     ESP_LOGI(TAG, "GPIO TO TOGGLE: %d", GPIO_PIN);
    // } else {
    //     ESP_LOGE(TAG, "Endpoint not found");
    // }
}
static void update_switch_value_from_input(int gpio_pin, uint16_t endpoint_id) {
    int gpio_index = get_gpio_index_by_endpoint(endpoint_id);
    if (gpio_index != -1){
        node_t *node = node::get();
        endpoint_t *endpoint = endpoint::get(node, endpoint_id);
        cluster_t *cluster = cluster::get(endpoint, OnOff::Id);
        attribute_t *attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);

        ESP_LOGI(TAG, "value %d, gpio %d", !val.val.b, gpio_pin);

        esp_matter_attr_val_t endpoint_value;    
        endpoint_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_BOOLEAN;
        endpoint_value.val.b = !val.val.b;

        attribute::update(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &endpoint_value);
        // attribute::set_val(attribute, &endpoint_value);
        gpio_set_level((gpio_num_t)gpio_pin, val.val.b);
    }
}
static void IRAM_ATTR input_isr_handler(void *arg) {
    gpio_isr_data_t *isr_data = (gpio_isr_data_t *)arg;
    int gpio_pin = isr_data->gpio_pin;
    int endpoint_id = isr_data->endpoint_id;

    int index = get_gpio_index_by_endpoint(gpio_pin);
    if (index != -1) {
        button_is_pressed[index] = gpio_get_level((gpio_num_t)gpio_pin) == 0;
        if (button_is_pressed[index] != button_was_pressed[index]) {
            gpio_isr_data_t evt;
            evt.gpio_pin = gpio_pin;
            evt.endpoint_id = endpoint_id;
            xQueueSendFromISR(gpio_evt_queue, &evt, NULL);
            button_was_pressed[index] = button_is_pressed[index];
        }
    }
}
static void gpio_task(void *arg) {
    gpio_isr_data_t evt;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &evt, portMAX_DELAY)) {
            int gpio_index = get_gpio_index_by_endpoint(evt.endpoint_id);
            if (gpio_index != -1) {
                update_switch_value_from_input(evt.gpio_pin, evt.endpoint_id);
            }
        }
    }
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
esp_err_t create_plug(int gpio_pin, node_t* node) {
    esp_err_t err = ESP_OK;
    driver_handle handle = switch_init(gpio_pin);
    on_off_plugin_unit::config_t config;
    config.on_off.on_off = DEFAULT_POWER;
    config.on_off.lighting.start_up_on_off = nullptr;
    endpoint_t *endpoint = on_off_plugin_unit::create(node, &config, ENDPOINT_FLAG_NONE, handle);
    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create switch endpoint for %d", gpio_pin);
        err = ESP_FAIL;
        return err;
    }

    for (int i = 0; i < configured_plugs; i++) {
        if (plug_unit_list[i].gpio_pin == gpio_pin) {
            ESP_LOGI(TAG, "Switch already configured: %d", endpoint::get_id(endpoint));
            return ESP_OK;
        }
    }

    if (configured_plugs < MAX_CONFIGURABLE_PLUGS) {
        plug_unit_list[configured_plugs].gpio_pin = gpio_pin;
        plug_unit_list[configured_plugs].endpoint_id = endpoint::get_id(endpoint);
        driver_plug_unit_set_defaults(endpoint::get_id(endpoint), gpio_pin);
        configured_plugs++;
    } else {
        ESP_LOGI(TAG, "Cannot configure more plugs");
        err = ESP_FAIL;
        return err;
    }


    static uint16_t plug_endpoint_id = 0;
    plug_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Plug created with endpoint_id %d", plug_endpoint_id);

    cluster::fixed_label::config_t fl_config;
    cluster::fixed_label::create(endpoint, &fl_config, CLUSTER_FLAG_SERVER);

    // static one input
    if (gpio_pin == 13) {
        input_switch_init(GPIO_NUM_27, plug_endpoint_id);
        // gpio_set_intr_type(GPIO_NUM_27, GPIO_INTR_ANYEDGE);
        gpio_set_intr_type(GPIO_NUM_27, GPIO_INTR_NEGEDGE);
        gpio_install_isr_service(0);

        gpio_isr_data_t* isr_data = (gpio_isr_data_t*) malloc(sizeof(gpio_isr_data_t));
        if (isr_data == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for ISR data");
            return ESP_ERR_NO_MEM;
        }
        isr_data->gpio_pin = gpio_pin;
        isr_data->endpoint_id = plug_endpoint_id;

        gpio_isr_handler_add(GPIO_NUM_27, input_isr_handler, (void*) isr_data);
        gpio_evt_queue = xQueueCreate(10, sizeof(gpio_isr_data_t));
        xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 10, NULL);
    }
    return err;
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

    iot_button_register_cb((button_handle_t)handle, BUTTON_PRESS_DOWN, driver_button_toggle_cb, NULL);

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