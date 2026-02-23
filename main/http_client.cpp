#include "http_client.h"
#include "config.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

static const char *TAG = "HTTP_CLIENT";

static char s_last_error[128] = {0};

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Print response data
                if (evt->data_len < 128) {
                    char response[128] = {0};
                    memcpy(response, evt->data, evt->data_len);
                    ESP_LOGI(TAG, "Response: %s", response);
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t http_client_init(void)
{
    ESP_LOGI(TAG, "HTTP client initialized");
    return ESP_OK;
}

esp_err_t http_client_send_scan_data(const scan_data_t* data)
{
    if (data == NULL) {
        strcpy(s_last_error, "Invalid scan data");
        return ESP_ERR_INVALID_ARG;
    }

    // Create JSON payload
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        strcpy(s_last_error, "Failed to create JSON object");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(json, "epc", data->epc);
    cJSON_AddStringToObject(json, "timestamp", data->timestamp);
    cJSON_AddNumberToObject(json, "rssi", data->rssi);
    cJSON_AddNumberToObject(json, "antenna", data->antenna);

    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        cJSON_Delete(json);
        strcpy(s_last_error, "Failed to print JSON");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Sending scan data: %s", json_string);

    // Build URL
    char url[256];
    snprintf(url, sizeof(url), "%s%s", SERVER_URL, SERVER_ENDPOINT);

    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.timeout_ms = HTTP_TIMEOUT_MS;
    config.method = HTTP_METHOD_POST;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(json_string);
        cJSON_Delete(json);
        strcpy(s_last_error, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (strlen(API_KEY) > 0) {
        char auth_header[128];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", API_KEY);
        esp_http_client_set_header(client, "Authorization", auth_header);
    }

    // Set POST data
    esp_http_client_set_post_field(client, json_string, strlen(json_string));

    // Perform request with retries
    esp_err_t ret = ESP_FAIL;
    for (int i = 0; i < HTTP_RETRY_COUNT; i++) {
        ret = esp_http_client_perform(client);
        
        if (ret == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            int content_length = esp_http_client_get_content_length(client);
            
            ESP_LOGI(TAG, "HTTP Status = %d, content_length = %d", status_code, content_length);
            
            if (status_code == 200 || status_code == 201) {
                ret = ESP_OK;
                break;
            } else {
                snprintf(s_last_error, sizeof(s_last_error), "HTTP error: %d", status_code);
                ret = ESP_FAIL;
            }
        } else {
            snprintf(s_last_error, sizeof(s_last_error), "HTTP request failed: %s", esp_err_to_name(ret));
        }

        if (i < HTTP_RETRY_COUNT - 1) {
            ESP_LOGW(TAG, "Retrying HTTP request (%d/%d)...", i + 1, HTTP_RETRY_COUNT);
            vTaskDelay(pdMS_TO_TICKS(1000 * (i + 1))); // Exponential backoff
        }
    }

    esp_http_client_cleanup(client);
    free(json_string);
    cJSON_Delete(json);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Successfully sent scan data to server");
    } else {
        ESP_LOGE(TAG, "Failed to send scan data: %s", s_last_error);
    }

    return ret;
}

const char* http_client_get_last_error(void)
{
    return s_last_error;
}
