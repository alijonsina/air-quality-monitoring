#include "mqtt_manager.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stdio.h>
#include "secrets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static esp_mqtt_client_handle_t mqtt_client = NULL;
static const char *TAG = "MQTT_MANAGER";

static bool mqtt_connected = false;


static void mqtt_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

esp_err_t mqtt_init(void) {
    esp_mqtt_client_config_t client_config = {
        .broker = {
            .address = {
                .uri = MQTT_BROKER_URL,
            },
        },
    };

    mqtt_client = esp_mqtt_client_init(&client_config);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialise MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

static void mqtt_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
    
    (void)arg;
    (void)event_base;
    (void)event_id;   
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            ESP_LOGI(TAG, "MQTT connected to broker");
            break;
        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            ESP_LOGW(TAG, "MQTT disconnected from broker");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "Message acknowledged by broker, msg_id=%d", event->msg_id);
            break;
        default:
            break;
    }
}

esp_err_t mqtt_publish(int node_id, float ppm, float temperature, float humidity) {
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT not connected, dropping reading");
        return ESP_FAIL;
    }

    char payload[128];
    char topic[64];

    snprintf(topic, sizeof(topic), "airquality/node%d/sensors", node_id);
    snprintf(payload, sizeof(payload), "{\"node\":%d,\"ppm\":%.2f,\"temperature\":%.2f,\"humidity\":%.2f}", node_id, ppm, temperature, humidity);

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Publish failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published to %s: %s", topic, payload);
    return ESP_OK;
}

bool is_mqtt_connected(void){
    return mqtt_connected;
}

esp_err_t mqtt_deinit(void) {
    esp_err_t ret;

    ret = esp_mqtt_client_disconnect(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT disconnect failed: %s", esp_err_to_name(ret));
    }

    ret = esp_mqtt_client_stop(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT stop failed: %s", esp_err_to_name(ret));
    }

    ret = esp_mqtt_client_destroy(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT destroy failed: %s", esp_err_to_name(ret));
        return ret;
    }

    mqtt_client = NULL;
    mqtt_connected = false;
    return ESP_OK;
}
