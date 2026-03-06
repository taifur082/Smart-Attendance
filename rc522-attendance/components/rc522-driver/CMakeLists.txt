idf_component_register(
    SRCS "src/rc522.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos esp_common
)
