idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES mq135 dht22 wifi_manager mqtt_manager config_manager app_update
    PRIV_REQUIRES log
)
