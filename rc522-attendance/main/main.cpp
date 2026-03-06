#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "rc522.h"
#include "config.h"
#include "wifi_manager.h"
#include "http_client.h"

static const char *TAG = "RC522_SCANNER";

// Session-based duplicate prevention: each UID is sent to the server at most once per device session.
#define SENT_UID_LIST_SIZE 256
static char s_sent_uids[SENT_UID_LIST_SIZE][65];
static int s_sent_count = 0;
static int s_sent_next = 0; // circular overwrite index when list is full

static bool already_sent_uid(const char *uid)
{
    for (int i = 0; i < s_sent_count; i++) {
        if (strcmp(s_sent_uids[i], uid) == 0) {
            return true;
        }
    }
    return false;
}

static void mark_uid_sent(const char *uid)
{
    if (s_sent_count < SENT_UID_LIST_SIZE) {
        strncpy(s_sent_uids[s_sent_count], uid, sizeof(s_sent_uids[0]) - 1);
        s_sent_uids[s_sent_count][sizeof(s_sent_uids[0]) - 1] = '\0';
        s_sent_count++;
    } else {
        strncpy(s_sent_uids[s_sent_next], uid, sizeof(s_sent_uids[0]) - 1);
        s_sent_uids[s_sent_next][sizeof(s_sent_uids[0]) - 1] = '\0';
        s_sent_next = (s_sent_next + 1) % SENT_UID_LIST_SIZE;
    }
}

static void get_timestamp_string(char *buf, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%S", &timeinfo);
}

static esp_err_t send_scan_to_server(const char *uid_hex)
{
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, skipping server send");
        return ESP_ERR_INVALID_STATE;
    }

    scan_data_t scan_data = {};
    strncpy(scan_data.epc, uid_hex, sizeof(scan_data.epc) - 1);
    get_timestamp_string(scan_data.timestamp, sizeof(scan_data.timestamp));
    scan_data.rssi    = 0; // RC522 does not expose RSSI
    scan_data.antenna = 1;

    esp_err_t ret = http_client_send_scan_data(&scan_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send scan data: %s", http_client_get_last_error());
    }
    return ret;
}

extern "C" void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "ESP32-C6 RC522 Smart Attendance System");
    ESP_LOGI(TAG, "=======================================");

    // NVS init (required by WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // WiFi init
    ESP_LOGI(TAG, "Initializing WiFi...");
    if (wifi_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager!");
        return;
    }

    ESP_LOGI(TAG, "Connecting to WiFi: %s", WIFI_SSID);
    if (wifi_manager_start(WIFI_SSID, WIFI_PASSWORD) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi! Device will continue offline.");
    } else {
        char ip_str[16];
        wifi_manager_get_ip(ip_str);
        ESP_LOGI(TAG, "WiFi connected! IP: %s", ip_str);
    }

    http_client_init();

    // SNTP time sync
    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    int sntp_retry = 0;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++sntp_retry < 10) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/10)", sntp_retry);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        ESP_LOGI(TAG, "System time synchronized");
    } else {
        ESP_LOGW(TAG, "System time not synchronized, using local time");
    }

    // RC522 init
    ESP_LOGI(TAG, "Initializing RC522 RFID reader...");
    if (rc522_init() != ESP_OK) {
        ESP_LOGE(TAG, "RC522 initialization failed! Check wiring and menuconfig pin settings.");
        return;
    }

    ESP_LOGI(TAG, "Server: %s%s", SERVER_URL, SERVER_ENDPOINT);
    ESP_LOGI(TAG, "Ready — hold a MIFARE card close to the reader.");
    ESP_LOGI(TAG, "=======================================");

    // Scan loop
    int scan_count = 0;
    while (true) {
        scan_count++;

        // Periodic WiFi reconnect if needed
        if (!wifi_manager_is_connected() && (scan_count % 20 == 0)) {
            ESP_LOGW(TAG, "WiFi disconnected, attempting reconnect...");
            wifi_manager_start(WIFI_SSID, WIFI_PASSWORD);
        }

        uint8_t uid_bytes[RC522_UID_MAX_BYTES];
        uint8_t uid_len = 0;

        if (rc522_detect_uid(uid_bytes, &uid_len)) {
            char uid_hex[RC522_UID_MAX_BYTES * 2 + 1];
            rc522_uid_to_hex(uid_bytes, uid_len, uid_hex);

            ESP_LOGI(TAG, "[Scan #%d] Card detected: UID=%s (%d bytes)", scan_count, uid_hex, uid_len);

            if (already_sent_uid(uid_hex)) {
                ESP_LOGI(TAG, "  Skipped (already sent this session)");
            } else {
                ESP_LOGI(TAG, "  Sending to server...");
                if (send_scan_to_server(uid_hex) == ESP_OK) {
                    mark_uid_sent(uid_hex);
                    ESP_LOGI(TAG, "  Sent OK");
                }
            }

            // Brief delay after a card read to avoid re-reading the same card immediately
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            // No card — short poll interval
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}
