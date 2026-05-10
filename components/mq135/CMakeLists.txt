idf_component_register(
    SRCS "mq135.c"
    INCLUDE_DIRS "."
    REQUIRES esp_adc
    PRIV_REQUIRES log
)
