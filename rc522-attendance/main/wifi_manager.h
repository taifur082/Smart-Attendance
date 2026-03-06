#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi in STA mode
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Start WiFi connection with retry logic
 * 
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_start(const char* ssid, const char* password);

/**
 * @brief Stop WiFi connection
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_stop(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get WiFi RSSI (signal strength)
 * 
 * @return RSSI value, 0 if not connected
 */
int8_t wifi_manager_get_rssi(void);

/**
 * @brief Get IP address as string
 * 
 * @param ip_str Buffer to store IP address string (must be at least 16 bytes)
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_ip(char* ip_str);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
