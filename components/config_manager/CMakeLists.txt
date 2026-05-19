idf_component_register(
    SRCS "config_manager.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash
    PRIV_REQUIRES log
)
