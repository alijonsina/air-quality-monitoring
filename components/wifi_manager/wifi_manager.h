#pragma once

#include "esp_err.h"

esp_err_t wifi_manager_init(void);

bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_deinit(void);
