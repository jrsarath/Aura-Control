#pragma once

// System Configuration
#define SYSTEM_TASK_STACK_SIZE     4096
#define SYSTEM_TASK_PRIORITY       5
#define SYSTEM_WATCHDOG_TIMEOUT_MS 5000

// OTA Configuration
#define OTA_TASK_STACK_SIZE        8192
#define OTA_TASK_PRIORITY         5
#define OTA_CHECK_INTERVAL_MS     3600000  // Check for updates every hour
#define OTA_FIRMWARE_TIMEOUT_MS   300000   // 5 minutes timeout for firmware download
#define OTA_MAX_RETRIES          3
#define OTA_RETRY_DELAY_MS       5000
#define OTA_BUFFER_SIZE          1024
#define OTA_ROLLBACK_ENABLED     1         // Enable rollback on failed boot
#define OTA_UPDATE_URL           "https://ota.48studios.com/aura/control/firmware.bin"

// Logging Configuration
#define LOG_BUFFER_SIZE            1024
#define MAX_LOG_FILES              5
#define MAX_LOG_FILE_SIZE          (1024 * 1024) // 1MB

// Matter Configuration
#define MATTER_MAX_ENDPOINTS       16
#define COMMISSIONING_TIMEOUT_SEC  300 

// PLUG Configuration
#define MAX_CONFIGURABLE_PLUGS 8
#define DEFAULT_POWER false
#define DEBOUNCE_DELAY_MS 1000
