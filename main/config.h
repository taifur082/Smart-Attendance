#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "Taifur"
#define WIFI_PASSWORD "12345678"
#define WIFI_MAX_RETRY 5

// Server Configuration
#define SERVER_URL "http://192.168.43.222:5000"
#define SERVER_ENDPOINT "/api/attendance/scan"
#define API_KEY "noOUi8kk6UrspSfHh3aTdDOfU710RwTSdYHpqASRwBs"  // For authentication

// HTTP Configuration
#define HTTP_TIMEOUT_MS 10000
#define HTTP_RETRY_COUNT 3

// Scan Configuration
// Duplicate prevention: each EPC is sent to server at most once per device session (see main.cpp).

// Queue Configuration
#define MAX_QUEUE_SIZE 50  // Maximum number of queued scan events

#endif // CONFIG_H
