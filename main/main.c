#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "mq135.h"
#include "dht22.h"
#include "mqtt_manager.h"

static const char *TAG = "AQM";
#define NODE_ID 1
#define SENSOR_READ_DELAY 2000

void app_main(void) {
    ESP_LOGI(TAG, "Starting the app:\n");
    ESP_LOGI(TAG, "Starting Wifi Manager:\n");
    esp_err_t ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failure to initialize Wifi Manager: %s", esp_err_to_name(ret));
        return;
    }

    ret = mqtt_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failure to initialize MQTT Manager: %s", esp_err_to_name(ret));
        return;
    }

    ret = mqtt_publish(NODE_ID, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failure to publish via MQTT: %s\n", esp_err_to_name(ret));
    }


    ret = dht22_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failure to initialize DHT22: %s", esp_err_to_name(ret));
        return;
    }

    ret = mq135_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failure to initialize MQ135: %s", esp_err_to_name(ret));
        return;
    }

    while(1) {
        float humidity = 0, temperature = 0, ppm = 0; 
        ret = dht22_read_value(&humidity, &temperature);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failure to read DHT22: %s\n", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_DELAY));
            continue;
        }   
        
        ret = mq135_read_ppm(temperature, humidity, &ppm);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failure to read MQ135: %s\n", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_DELAY));
            continue;
        } 

        ESP_LOGI(TAG, "PPM = %f, Temperature = %f, Humidity = %f \n", ppm, temperature, humidity);
        
        ret = mqtt_publish(NODE_ID, ppm, temperature, humidity);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failure to publish via MQTT: %s\n", esp_err_to_name(ret));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_DELAY));
    }
}
