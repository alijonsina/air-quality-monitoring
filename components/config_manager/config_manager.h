#pragma once

#include "esp_err.h"

esp_err_t config_init(void);

int return_node_id(void);
esp_err_t config_set_node_id(int node_id);

esp_err_t config_set_rzero(float rzero);
float config_get_rzero(void);

esp_err_t config_set_read_interval(int ms);
int config_get_read_interval(void);
