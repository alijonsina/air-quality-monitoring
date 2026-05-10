#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#define DHT22_PIN GPIO_NUM_2

esp_err_t dht22_init(void);

esp_err_t dht22_read_value(float *humidity, float* temperature);
