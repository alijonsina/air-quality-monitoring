idf_component_register(
    SRCS "mqtt_manager.c"
    INCLUDE_DIRS "."
    REQUIRES espressif__mqtt
    PRIV_REQUIRES log
)
