set(SOURCES ./src/common.cpp
            ./src/hal-serial.cpp
            ./src/910driver.cpp
            ./src/903driver.cpp 
            ./src/driver.cpp
            ./src/main.cpp 
)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . include
                    REQUIRES freertos esp_common driver)
