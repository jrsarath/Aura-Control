#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include <esp_matter.h>
#include <esp_matter_ota.h>
#include <esp_matter_console.h>
#include <driver/gpio.h>
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif
#include <app/server/Server.h>
#include <app/server/CommissioningWindowManager.h>

#include "includes/variables.hpp"
#include "includes/driver.hpp"
#include "includes/utils.hpp"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "matter";

/**
 * @brief Application event callback
 * 
 * @param event Pointer to the ChipDeviceEvent
 * @param arg   Argument passed during registration
 */
static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg) {
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
            ESP_LOGI(TAG, "Interface IP Address Changed");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
            ESP_LOGI(TAG, "Commissioning complete");
            break;

        case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
            ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
            ESP_LOGI(TAG, "Commissioning session started");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
            ESP_LOGI(TAG, "Commissioning session stopped");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
            ESP_LOGI(TAG, "Commissioning window opened");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
            ESP_LOGI(TAG, "Commissioning window closed");
            break;

        default:
            break;
    }
}

/**
 * @brief  Identification callback
 * 
 * @param type        Type of the identification event
 * @param endpoint_id Endpoint ID of the identified device
 * @param effect_id   Effect ID
 * @param effect_variant Effect variant
 * @param priv_data   Private data pointer
 */
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    // device_identifier_cb();
    return ESP_OK;
}

/**
 * @brief Attribute update callback
 * 
 * @param type          Type of the callback (PRE_UPDATE/POST_UPDATE)
 * @param endpoint_id   Endpoint ID of the attribute
 * @param cluster_id    Cluster ID of the attribute
 * @param attribute_id  Attribute ID
 * @param val           Pointer to the attribute value
 * @param priv_data     Private data pointer
 */
static esp_err_t app_attribute_update_cb(callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data) {
    esp_err_t err = ESP_OK;
    if (type == PRE_UPDATE) {
        driver_handle handle = (driver_handle)priv_data;
        err = driver_attribute_update(handle, endpoint_id, cluster_id, attribute_id, val);
    }
    return err;
}

/**
 * @brief Application main entry point
 */
extern "C" void app_main() {
    esp_err_t err = ESP_OK;
    
    // Initialize NVS
    err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %d", err);
        return;
    }

    // Initialize driver
    err = driver_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize driver: %d", err);
        return;
    }

    driver_handle button_handle = driver_button_init();
    if (!button_handle) {
        ESP_LOGE(TAG, "Failed to initialize button");
        return;
    }
    app_reset_button_register(button_handle);

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return;
    }

    // plug plug;
    // plug.output_gpio_pin = (gpio_num_t)CONFIG_SWITCH_1_OUTPUT_PIN;
    // plug.input_gpio_pin = (gpio_num_t)CONFIG_SWITCH_1_INPUT_PIN;
    // create_plug(&plug, node);

    // Setup Switches
    // for (int i = 0; i < MAX_CONFIGURABLE_PLUGS; ++i) {
    //     if (plugs[i].output_gpio_pin > 0) {
    //         esp_err_t created_plug = create_plug(const_cast<plug *>(&plugs[i]), node);
    //         if (created_plug != ESP_OK) {
    //             ESP_LOGW(TAG, "Failed to create plug for pin %d", plugs[i].output_gpio_pin);
    //         }
    //         // if (plug.endpoint_id != -1) {
    //         //     int inputPin = find_input_pin_by_output_pin(plug.gpio_pin);
    //         //     // if (inputPin > 0) {
    //         //     //     if (!input_switch_init(inputPin, plug.endpoint_id)) {
    //         //     //         ESP_LOGW(TAG, "Failed to initialize input switch for pin %d", inputPin);
    //         //     //     }
    //         //     // }
    //         // } else {
    //         //     ESP_LOGW(TAG, "Failed to create plug for pin %d", outputPins[i]);
    //         // }
    //     }
    // }

    #if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        // Set OpenThread platform config
        esp_openthread_platform_config_t config = {
            .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
            .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
        };
        set_openthread_platform_config(&config);
    #endif

    // Matter start
    err = esp_matter::start(app_event_cb);
    abort_on_failure(err == ESP_OK, TAG, "Failed to start Matter, err:%d", err);

    #if CONFIG_ENABLE_CHIP_SHELL
        esp_matter::console::diagnostics_register_commands();
        esp_matter::console::wifi_register_commands();
        esp_matter::console::factoryreset_register_commands();
    #if CONFIG_OPENTHREAD_CLI
        esp_matter::console::otcli_register_commands();
    #endif
        esp_matter::console::init();
    #endif
}