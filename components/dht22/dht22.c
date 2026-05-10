#include "dht22.h"
#include "dht.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "DHT22";

esp_err_t dht22_init(void) {
    ESP_LOGI(TAG, "DHT22 initialised on GPIO %d\n", DHT22_PIN);
    return ESP_OK;
}

esp_err_t dht22_read_value(float *humidity, float *temperature) {
    esp_err_t result = dht_read_float_data(DHT_TYPE_AM2301, DHT22_PIN,
                                           humidity, temperature);

    if (result == ESP_ERR_INVALID_CRC) {
        ESP_LOGW(TAG, "CRC error, retrying...\n");
        vTaskDelay(pdMS_TO_TICKS(100));
        result = dht_read_float_data(DHT_TYPE_AM2301, DHT22_PIN,
                                     humidity, temperature);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Retry failed: %s\n", esp_err_to_name(result));
        }
    } else if (result != ESP_OK) {
        ESP_LOGE(TAG, "DHT22 read failed: %s\n", esp_err_to_name(result));
    }

    return result;
}
