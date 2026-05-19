idf_component_register(
    SRCS "ota_manager.c"
    INCLUDE_DIRS "."
    REQUIRES esp_https_ota app_update esp_http_client
    PRIV_REQUIRES log
)
