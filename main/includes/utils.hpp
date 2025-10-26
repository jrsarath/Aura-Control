#pragma once

#include <stdbool.h>
#include <esp_err.h>

/**
 * @brief Log an error message and abort the program if the condition is false.
 *
 * @param ok Condition to check.
 * @param tag Tag to use in the log message.
 * @param fmt Format string for the log message.
 * @param ... Additional arguments for the format string.
 * @example abort_on_failure(err == ESP_OK, TAG, "Failed to start Matter, err:%d", err);
 */
void abort_on_failure(bool ok, const char *tag, const char *fmt, ...);

/**
 * @brief Register the reset button callbacks
 * 
 * @param handle Button handle pointer
 * @return esp_err_t 
 */
esp_err_t app_reset_button_register(void *handle);
