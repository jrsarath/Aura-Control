#include "includes/variables.hpp"

// plug unit endpoint configurations
const plug plugs[MAX_CONFIGURABLE_PLUGS] = {
    { (gpio_num_t)CONFIG_SWITCH_1_OUTPUT_PIN, (gpio_num_t)CONFIG_SWITCH_1_INPUT_PIN },
    { (gpio_num_t)CONFIG_SWITCH_2_OUTPUT_PIN, (gpio_num_t)CONFIG_SWITCH_2_INPUT_PIN },
    { (gpio_num_t)CONFIG_SWITCH_3_OUTPUT_PIN, (gpio_num_t)CONFIG_SWITCH_3_INPUT_PIN },
    { (gpio_num_t)CONFIG_SWITCH_4_OUTPUT_PIN, (gpio_num_t)CONFIG_SWITCH_4_INPUT_PIN },
    { (gpio_num_t)CONFIG_SWITCH_5_OUTPUT_PIN, (gpio_num_t)CONFIG_SWITCH_5_INPUT_PIN },
    { (gpio_num_t)CONFIG_SWITCH_6_OUTPUT_PIN, (gpio_num_t)CONFIG_SWITCH_6_INPUT_PIN },
    { (gpio_num_t)CONFIG_SWITCH_7_OUTPUT_PIN, (gpio_num_t)CONFIG_SWITCH_7_INPUT_PIN },
    { (gpio_num_t)CONFIG_SWITCH_8_OUTPUT_PIN, (gpio_num_t)CONFIG_SWITCH_8_INPUT_PIN },
};