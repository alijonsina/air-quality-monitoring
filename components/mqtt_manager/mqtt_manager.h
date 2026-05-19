#pragma once

#include "esp_err.h"

esp_err_t mqtt_init(int nodeID);
esp_err_t mqtt_publish(int64_t timestamp, float ppm, float temperature, float humidity);
esp_err_t mqtt_publish_update_status(const char* status);
bool is_mqtt_connected(void);
esp_err_t mqtt_deinit(void);
