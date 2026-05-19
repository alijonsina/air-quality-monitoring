#include "mqtt_manager.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include "secrets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_manager.h"
#include "ota_manager.h"
#include "esp_ota_ops.h"

static esp_mqtt_client_handle_t mqtt_client = NULL;
static const char *TAG = "MQTT_MANAGER";
static int mqtt_node_id = 0;
static bool mqtt_connected = false;

static void mqtt_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

esp_err_t mqtt_publish_update_status(const char *status); 

esp_err_t mqtt_init(int node_id) {
    mqtt_node_id = node_id;

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
        case MQTT_EVENT_CONNECTED: {
            mqtt_connected = true;
            ESP_LOGI(TAG, "MQTT connected to broker");

            char ota_topic[64];
            char config_topic[64];

            if (mqtt_node_id == 0) {
                esp_mqtt_client_subscribe(mqtt_client, "airquality/unassigned/config", 1);
                ESP_LOGI(TAG, "Subscribed to unassigned config topic");
            } else {
                snprintf(ota_topic, sizeof(ota_topic), "airquality/node%d/ota", mqtt_node_id);
                snprintf(config_topic, sizeof(config_topic), "airquality/node%d/config", mqtt_node_id);
                esp_mqtt_client_subscribe(mqtt_client, ota_topic, 1);
                esp_mqtt_client_subscribe(mqtt_client, config_topic, 1);
                ESP_LOGI(TAG, "Subscribed to %s and %s", ota_topic, config_topic);
            }

            esp_ota_img_states_t ota_state;
            const esp_partition_t *running = esp_ota_get_running_partition();
            if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
                if (ota_state == ESP_OTA_IMG_VALID) {
                    mqtt_publish_update_status("ota_success");
                }
            }
            break;
        }
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

        case MQTT_EVENT_DATA: {
            char topic[128] = {0};
            char data[256] = {0};
            snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
            snprintf(data, sizeof(data), "%.*s", event->data_len, event->data);

            ESP_LOGI(TAG, "Received on topic %s: %s", topic, data);

            char expected_ota[64];
            snprintf(expected_ota, sizeof(expected_ota), "airquality/node%d/ota", mqtt_node_id);

            if (strcmp(topic, expected_ota) == 0) {
                ESP_LOGI(TAG, "OTA update requested, URL: %s", data);
                mqtt_publish_update_status("ota_start");
                esp_err_t ret = ota_manager_start(data);
                if (ret != ESP_OK) {
                    mqtt_publish_update_status("ota_failed");
                }
                break;
            }

            if (strcmp(topic, "airquality/unassigned/config") == 0) {
                int new_id = 0;
                if (sscanf(data, "{\"assign_id\":%d}", &new_id) == 1) {
                    ESP_LOGI(TAG, "Received node ID assignment: %d", new_id);

                    config_set_node_id(new_id);

                    char payload[64];
                    snprintf(payload, sizeof(payload), "{\"node\":%d,\"status\":\"id_assigned\"}", new_id);
                    esp_mqtt_client_publish(mqtt_client, "airquality/unassigned/status", payload, 0, 1, 0);

                    vTaskDelay(pdMS_TO_TICKS(2000));
                    ESP_LOGI(TAG, "Rebooting with new node ID: %d", new_id);
                    esp_restart();
                } else {
                    ESP_LOGE(TAG, "Failed to parse assign_id from: %s", data);
                }
                break;
            }
            break;
        }
        default:
            break;
    }
}

esp_err_t mqtt_publish(int64_t timestamp, float ppm, float temperature, float humidity) {
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT not connected, dropping reading");
        return ESP_FAIL;
    }

    char payload[128];
    char topic[64];

    snprintf(topic, sizeof(topic), "airquality/node%d/sensors", mqtt_node_id);
    snprintf(payload, sizeof(payload), "{\"node\":%d,\"timestamp\":%lld,\"ppm\":%.2f," "\"temperature\":%.2f,\"humidity\":%.2f}", mqtt_node_id, timestamp, ppm, temperature, humidity);

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Publish failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published to %s: %s", topic, payload);
    return ESP_OK;
}

esp_err_t mqtt_publish_update_status(const char *status) {
    if (!mqtt_connected) return ESP_FAIL;

    char topic[64];
    char payload[128];

    snprintf(topic, sizeof(topic), "airquality/node%d/status", mqtt_node_id);
    snprintf(payload, sizeof(payload), "{\"node\":%d,\"status\":\"%s\"}", mqtt_node_id, status);

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Status publish failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool is_mqtt_connected(void) {
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
