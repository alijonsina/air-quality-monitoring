#include "mq135.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "config_manager.h"
#include <math.h>

#define CORA 0.00035f
#define CORB 0.02718f
#define CORC 1.39538f
#define CORD 0.0018f
#define CORE -0.003333333f
#define CORF -0.001923077f
#define CORG 1.130128205f

static const char *TAG = "MQ135";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;
static bool calibrated = false;

esp_err_t mq135_init(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = MQ135_ADC_UNIT,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "New adc unit failure: %s\n", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = MQ135_ADC_BITWIDTH,
        .atten    = MQ135_ADC_ATTENUATION,
    };

    ret = adc_oneshot_config_channel(adc_handle, MQ135_ADC_CHANNEL, &chan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "New adc config channel failure: %s\n", esp_err_to_name(ret));
        return ret;
    }

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id  = MQ135_ADC_UNIT,
        .chan     = MQ135_ADC_CHANNEL,
        .atten    = MQ135_ADC_ATTENUATION,
        .bitwidth = MQ135_ADC_BITWIDTH,
    };

    if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK) {
        calibrated = true;
        ESP_LOGI(TAG, "ADC calibration enabled\n");
    } else {
        ESP_LOGW(TAG, "ADC calibration unavailable, using raw values\n");
    }

    ESP_LOGI(TAG, "MQ135 preheating for %d seconds...\n", MQ135_PREHEAT_MS / 1000);
    vTaskDelay(pdMS_TO_TICKS(MQ135_PREHEAT_MS));
    ESP_LOGI(TAG, "Preheat complete\n");

    return ESP_OK;
}

static esp_err_t mq135_read_raw(int *raw) {
    int sum     = 0;
    int samples = 64;

    for (int i = 0; i < samples; i++) {
        int sample = 0;
        esp_err_t ret = adc_oneshot_read(adc_handle, MQ135_ADC_CHANNEL, &sample);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ADC read failed at sample %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
        sum += sample;
    }
    *raw = sum / samples;
    return ESP_OK;
}

static esp_err_t mq135_read_voltage(int *voltage_mv) {
    int raw;
    esp_err_t ret = mq135_read_raw(&raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC read raw failed: %s\n", esp_err_to_name(ret));
        return ret;
    }

    if (calibrated) {
        ret = adc_cali_raw_to_voltage(cali_handle, raw, voltage_mv);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Voltage conversion failed: %s\n", esp_err_to_name(ret));
            return ret;
        }
    } else {
        *voltage_mv = (raw * 3300) / 4095;
    }    
    return ESP_OK;
}

static float get_correction_factor(float temperature, float humidity) {
    if (temperature < 20.0f) {
        return CORA * temperature * temperature - CORB * temperature + CORC - (humidity - 33.0f) * CORD;
    } else {
        return CORE * temperature + CORF * humidity + CORG;
    }
}

esp_err_t mq135_read_ppm(float temperature, float humidity, float *ppm) {
    int voltage_mv;
    esp_err_t ret = mq135_read_voltage(&voltage_mv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Voltage reading failed: %s \n", esp_err_to_name(ret));
        return ret;
    }
    float voltage_v = voltage_mv / 1000.0f;
    
    float sensor_resistance;
    sensor_resistance = ((VCC * RLOAD) / voltage_v) - RLOAD;

    float corrected_sensor_resistance = sensor_resistance / get_correction_factor(temperature, humidity);

    *ppm = MQ135_PARA * powf(corrected_sensor_resistance / config_get_rzero(), -MQ135_PARB);
    
    return ESP_OK;
}


