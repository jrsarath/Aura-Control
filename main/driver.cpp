#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <device.h>
#include <driver/gpio.h>

#include "includes/driver.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace chip::app::Clusters;

static const char *TAG = "driver";

static void driver_button_toggle_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Toggle button pressed");
}

driver_handle driver_button_init() {
    button_config_t config = button_driver_get_config();
    button_handle_t handle = iot_button_create(&config);
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, driver_button_toggle_cb, NULL);
    return (driver_handle)handle;
}