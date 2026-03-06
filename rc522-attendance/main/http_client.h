#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "esp_err.h"
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure to hold scan data for HTTP POST
 */
typedef struct {
    char epc[65];           // UID hex string (used as EPC equivalent)
    char timestamp[32];     // ISO 8601 timestamp
    int8_t rssi;            // Signal strength (0 for RC522; no RSSI available)
    uint8_t antenna;        // Antenna number (1 for single-antenna RC522)
} scan_data_t;

/**
 * @brief Initialize HTTP client
 * 
 * @return ESP_OK on success
 */
esp_err_t http_client_init(void);

/**
 * @brief Send scan data to server
 * 
 * @param data Scan data structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t http_client_send_scan_data(const scan_data_t* data);

/**
 * @brief Get last HTTP error message
 * 
 * @return Error message string
 */
const char* http_client_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // HTTP_CLIENT_H
