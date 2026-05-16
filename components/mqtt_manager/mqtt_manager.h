#pragma once

#include "esp_err.h"

esp_err_t mqtt_init(void);
esp_err_t mqtt_publish(int nodeID, float ppm, float temperature, float humidity);
bool is_mqtt_connected(void);
esp_err_t mqtt_deinit(void);
