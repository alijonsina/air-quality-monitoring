#pragma once

#include "esp_err.h"

esp_err_t config_init(void);
int return_node_id(void);
esp_err_t config_set_node_id(int node_id);

