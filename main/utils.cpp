#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_matter.h>
#include "iot_button.h"
#include "includes/utils.hpp"

static const char *TAG = "UTILS";
static bool perform_factory_reset = false;

/**
 * @brief Button factory reset pressed callback
 * 
 * @param arg 
 * @param data 
 */
static void button_factory_reset_pressed_cb(void *arg, void *data) {
    if (!perform_factory_reset) {
        ESP_LOGI(TAG, "Factory reset triggered. Release the button to start factory reset.");
        perform_factory_reset = true;
    }
}

/**
 * @brief Button factory reset released callback
 * 
 * @param arg 
 * @param data 
 */
static void button_factory_reset_released_cb(void *arg, void *data) {
    if (perform_factory_reset) {
        ESP_LOGI(TAG, "Starting factory reset");
        esp_matter::factory_reset();
        perform_factory_reset = false;
    }
}

/**
 * @brief Log an error message and abort the program if the condition is false.
 * 
 * @param ok Condition to check.
 * @param tag Tag to use in the log message.
 * @param fmt Format string for the log message.
 * @param ... Additional arguments for the format string.
 */
void abort_on_failure(bool ok, const char *tag, const char *fmt, ...) {
    if (ok) {
        return;
    }

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ESP_LOGE(tag, "%s", buf);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    abort();
}

/**
 * @brief Register the reset button callbacks
 * 
 * @param handle 
 * @return esp_err_t 
 */
esp_err_t app_reset_button_register(void *handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Handle cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    button_handle_t button_handle = (button_handle_t)handle;
    esp_err_t err = ESP_OK;
    err |= iot_button_register_cb(button_handle, BUTTON_LONG_PRESS_HOLD, NULL, button_factory_reset_pressed_cb, NULL);
    err |= iot_button_register_cb(button_handle, BUTTON_PRESS_UP, NULL, button_factory_reset_released_cb, NULL);
    return err;
}