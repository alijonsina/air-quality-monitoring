#include "config_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "CONFIG";
static const char *NVS_NAMESPACE = "config";
static const char *NVS_NODE_ID_KEY = "node_id";
static int node_id = 0;  

esp_err_t config_init(void) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    int32_t stored_id = 0;
    ret = nvs_get_i32(handle, NVS_NODE_ID_KEY, &stored_id);
    if (ret == ESP_OK) {
        node_id = (int)stored_id;
        ESP_LOGI(TAG, "Node ID loaded from NVS: %d", node_id);
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "No node ID in NVS, using default: %d", node_id);
    } else {
        ESP_LOGE(TAG, "NVS read error: %s", esp_err_to_name(ret));
    }

    nvs_close(handle);
    return ESP_OK;
}

int return_node_id(void) {
    return node_id;
}

esp_err_t config_set_node_id(int new_id) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_i32(handle, NVS_NODE_ID_KEY, (int32_t)new_id);
    if (ret != ESP_OK) {
        nvs_close(handle);
        return ret;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        node_id = new_id;
        ESP_LOGI(TAG, "Node ID saved to NVS: %d", new_id);
    }

    return ret;
}
