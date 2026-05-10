idf_component_register(
    SRCS "dht22.c"
    INCLUDE_DIRS "."
    REQUIRES dht
    PRIV_REQUIRES log
)
