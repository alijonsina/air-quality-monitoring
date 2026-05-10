#pragma once
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#define VCC 5.0f
#define MQ135_ADC_CHANNEL ADC_CHANNEL_0
#define MQ135_ADC_UNIT ADC_UNIT_1
#define MQ135_ADC_BITWIDTH ADC_BITWIDTH_12
#define MQ135_ADC_ATTENUATION ADC_ATTEN_DB_12

#define RZERO 76.63f
#define RLOAD 10.0f

#define MQ135_PREHEAT_MS 30000
#define MQ135_ATMOCO2 397.13f
#define MQ135_PARA 116.6020682f
#define MQ135_PARB 2.769034857f


esp_err_t mq135_init(void);
esp_err_t mq135_read_ppm(float temperature, float humidity, float *ppm); 
