#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "driver.h"
#include "dtypes.h"
#include "config.h"
#include "wifi_manager.h"
#include "http_client.h"

static const char *TAG = "EPC_SCANNER";

// Scan mode configuration flag
// Set to 1 for continuous scanning (infinite loop)
// Set to 0 for single scan mode (runs once and exits)
#define CONTINUOUS_SCAN_MODE 1

// Session-based duplicate prevention: each EPC is sent to server at most once per device session (until reboot).
#define SENT_EPC_LIST_SIZE 256
static char s_sent_epcs[SENT_EPC_LIST_SIZE][65];
static int s_sent_count = 0;
static int s_sent_next = 0;  // For circular overwrite when full

// Returns true if this EPC was already sent this session (should skip to avoid duplicate in DB).
static bool already_sent_epc(const char* epc)
{
    for (int i = 0; i < s_sent_count; i++) {
        if (strcmp(s_sent_epcs[i], epc) == 0) {
            return true;
        }
    }
    return false;
}

// Mark EPC as sent (call after successfully sending to server).
static void mark_epc_sent(const char* epc)
{
    if (s_sent_count < SENT_EPC_LIST_SIZE) {
        strncpy(s_sent_epcs[s_sent_count], epc, sizeof(s_sent_epcs[0]) - 1);
        s_sent_epcs[s_sent_count][sizeof(s_sent_epcs[0]) - 1] = '\0';
        s_sent_count++;
    } else {
        // Circular overwrite of oldest entry
        strncpy(s_sent_epcs[s_sent_next], epc, sizeof(s_sent_epcs[0]) - 1);
        s_sent_epcs[s_sent_next][sizeof(s_sent_epcs[0]) - 1] = '\0';
        s_sent_next = (s_sent_next + 1) % SENT_EPC_LIST_SIZE;
    }
}

// Get current timestamp as ISO 8601 string
static void get_timestamp_string(char* buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%S", &timeinfo);
}

// Send scan data to server. Returns ESP_OK on success (so caller can mark EPC as sent).
static esp_err_t send_scan_to_server(const char* epc, int8_t rssi, uint8_t antenna)
{
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, skipping server send");
        return ESP_ERR_INVALID_STATE;
    }
    
    scan_data_t scan_data = {};
    strncpy(scan_data.epc, epc, sizeof(scan_data.epc) - 1);
    get_timestamp_string(scan_data.timestamp, sizeof(scan_data.timestamp));
    scan_data.rssi = rssi;
    scan_data.antenna = antenna;
    
    esp_err_t ret = http_client_send_scan_data(&scan_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send scan data: %s", http_client_get_last_error());
    }
    return ret;
}

extern "C" void app_main(void)
{
    // Disable driver debug output for cleaner logs
    extern bool Debug_;
    Debug_ = false;
    
    // Add a small delay to ensure system is ready
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "ESP32-C6 Smart Attendance System");
    ESP_LOGI(TAG, "=================================");
    
    // Initialize NVS (required by WiFi before esp_wifi_init)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    if (wifi_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager!");
        return;
    }
    
    // Connect to WiFi
    ESP_LOGI(TAG, "Connecting to WiFi: %s", WIFI_SSID);
    if (wifi_manager_start(WIFI_SSID, WIFI_PASSWORD) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi!");
        ESP_LOGE(TAG, "Please check WiFi credentials in config.h");
        // Continue anyway - device can work offline and queue requests
    } else {
        char ip_str[16];
        wifi_manager_get_ip(ip_str);
        ESP_LOGI(TAG, "WiFi connected! IP: %s", ip_str);
    }
    
    // Initialize HTTP client
    http_client_init();
    
    // Initialize SNTP for accurate timestamps
    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    
    // Wait for time sync (up to 10 seconds)
    int retry = 0;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 10) {
        ESP_LOGI(TAG, "Waiting for system time to be set...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        ESP_LOGI(TAG, "System time synchronized");
    } else {
        ESP_LOGW(TAG, "System time not synchronized, using local time");
    }
    
    // Open UART connection to reader
    ESP_LOGI(TAG, "Initializing UHF Reader...");
    if (!OpenComPort("COMX", 57600)) {
        ESP_LOGE(TAG, "Failed to open reader port!");
        return;
    }
    // Give the reader time to power up and respond (avoids "Read command failed" on first commands)
    vTaskDelay(pdMS_TO_TICKS(1500));

    // Get and configure reader settings
    ReaderInfo ri;
    if (!GetSettings(&ri)) {
        ESP_LOGE(TAG, "Failed to get reader settings!");
        CloseComPort();
        return;
    }
    
    // Configure reader settings
    ri.ScanTime = 3;  // 300ms (3 * 100ms)
    ri.BeepOn = 1;    // Enable buzzer
    SetSettings(&ri);
    
    #if CONTINUOUS_SCAN_MODE
    ESP_LOGI(TAG, "Mode: CONTINUOUS | Interval: 3s");
    #else
    ESP_LOGI(TAG, "Mode: SINGLE SCAN");
    #endif
    ESP_LOGI(TAG, "Server: %s%s", SERVER_URL, SERVER_ENDPOINT);
    ESP_LOGI(TAG, "=================================");
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Scan loop - controlled by CONTINUOUS_SCAN_MODE flag
    int scan_count = 0;
    do {
        scan_count++;
        
        // Check WiFi connection status periodically
        if (!wifi_manager_is_connected() && (scan_count % 10 == 0)) {
            ESP_LOGW(TAG, "WiFi disconnected, attempting reconnect...");
            wifi_manager_start(WIFI_SSID, WIFI_PASSWORD);
        }
        
        // Perform inventory scan
        int tag_count = Inventory(false);  // false = no filter
        
        if (tag_count == 0) {
            ESP_LOGI(TAG, "[Scan #%d] No tags found", scan_count);
        } else {
            ESP_LOGI(TAG, "[Scan #%d] Found %d tag(s):", scan_count, tag_count);
            
            // Process each detected tag
            static char epc_hex[65];  // Static to avoid stack allocation
            for (int i = 0; i < tag_count; i++) {
                ScanResult sr;
                if (GetResult((unsigned char*)&sr, i)) {
                    if (sr.epclen > 0 && sr.epclen <= 32) {
                        // Convert bytes to hex string
                        memset(epc_hex, 0, sizeof(epc_hex));
                        for (int j = 0; j < sr.epclen; j++) {
                            snprintf(epc_hex + j*2, 3, "%02X", sr.epc[j]);
                        }
                        
                        ESP_LOGI(TAG, "  Tag %d: EPC=%s RSSI=%d Ant=%d", 
                                i + 1, epc_hex, sr.RSSI, sr.ant);
                        
                        // Send to server only if we haven't already sent this EPC this session (avoids duplicate DB entries).
                        if (already_sent_epc(epc_hex)) {
                            ESP_LOGI(TAG, "  Skipped (already sent this session)");
                        } else {
                            ESP_LOGI(TAG, "  Sending to server...");
                            if (send_scan_to_server(epc_hex, sr.RSSI, sr.ant) == ESP_OK) {
                                mark_epc_sent(epc_hex);
                            }
                        }
                    }
                }
            }
        }
        
        #if CONTINUOUS_SCAN_MODE
        // Wait 3 seconds between scans (continuous mode)
        vTaskDelay(pdMS_TO_TICKS(3000));
        #else
        // Single scan mode - exit after one scan
        break;
        #endif
    } while (CONTINUOUS_SCAN_MODE);
    
    // Cleanup
    CloseComPort();
    wifi_manager_stop();
    ESP_LOGI(TAG, "Done");
}