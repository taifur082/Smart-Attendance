idf_component_register(
    SRCS "main.cpp"
         "wifi_manager.cpp"
         "http_client.cpp"
    INCLUDE_DIRS .
    REQUIRES driver nvs_flash rc522-driver esp_wifi esp_http_client esp_netif lwip json esp_timer
)
