# Smart Attendance System

A complete IoT-based attendance tracking system using ESP32-C6, UHF RFID reader, Flask server, and SMS/WhatsApp notifications.

## System Overview

```
ESP32-C6 (Scanner) → WiFi → Flask Server → PostgreSQL Database
                                         ↓
                                    SMS Gateway API
                                    WhatsApp API (Green API)
```

## Project Structure

```
.
├── main/                    # ESP32-C6 firmware
│   ├── main.cpp            # Main application with scan loop
│   ├── wifi_manager.cpp/h  # WiFi connection management
│   ├── http_client.cpp/h   # HTTP client for server communication
│   └── config.h            # Configuration (WiFi, server URL, etc.)
├── server/                  # Flask backend server
│   ├── app.py              # Flask application entry point
│   ├── models.py           # Database models (SQLAlchemy)
│   ├── routes.py           # API endpoints
│   ├── config.py           # Configuration management
│   ├── services/           # Notification services
│   │   ├── sms_service.py
│   │   └── whatsapp_service.py
│   └── database/           # Database initialization
└── components/             # ESP-IDF components
    └── UHF-driver/      # UHF RFID reader driver
```

## Hardware Requirements

- ESP32-C6 development board
- J4210U UHF RFID Reader module
- UART connection between ESP32-C6 and RFID reader
- WiFi access point

## ESP32-C6 Firmware Setup

### Prerequisites

- ESP-IDF v5.1 or later
- Python 3.8+
- CMake 3.16+

### Configuration

1. **Edit `main/config.h`:**
   ```c
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   #define SERVER_URL "http://192.168.1.100:5000"
   #define API_KEY "your-api-key-here"
   ```

2. **Build and flash:**
   ```bash
   idf.py build
   idf.py flash monitor
   ```

### Features

- WiFi connection with automatic retry
- Continuous RFID tag scanning
- EPC debouncing (prevents duplicate scans)
- HTTP POST to server with JSON payload
- SNTP time synchronization
- Offline queue support (future enhancement)

### UHF reader "Read command failed"

If the ESP32 logs **"Read command failed"** and **"Failed to get reader settings!"**, the UHF reader is not responding on UART. Try:

1. **Wiring:** Connect ESP32 **TX** (GPIO 17 by default) to the reader’s **RX**, and ESP32 **RX** (GPIO 16) to the reader’s **TX**. If it still fails, try swapping TX and RX.
2. **Pins:** UART pins are configurable. Run `idf.py menuconfig` → **Component config** → **UHF RFID reader driver** and set **UART TX GPIO** / **UART RX GPIO** to match your board.
3. **Power:** A 1.5 s startup delay is applied after opening the UART; if your reader needs longer to power up, increase the delay in `main/main.cpp` after `OpenComPort(...)`.
4. **Baud rate:** The reader is opened at 57600. If your module uses 115200, change the baud in the `OpenComPort("COMX", 57600)` call in `main/main.cpp`.

## Server Setup

### Prerequisites

- Python 3.8+
- PostgreSQL 12+
- pip

### Installation

1. **Install dependencies:**
   ```bash
   cd server
   pip install -r requirements.txt
   ```

2. **Create PostgreSQL database:**
   ```sql
   CREATE DATABASE attendance_db;
   ```

3. **Configure environment:**
   ```bash
   cp .env.example .env
   # Edit .env with your configuration
   ```

4. **Initialize database:**
   ```bash
   python database/init_db.py --seed  # --seed adds test data
   ```

5. **Clear database (remove dummy/old data):**
   From the `server` directory:
   ```bash
   python -m database.clear_db              # Delete all rows (keeps tables)
   python -m database.clear_db --recreate   # Drop all tables and recreate (full reset)
   ```

6. **Run server:**
   ```bash
   python app.py
   ```

### Configuration

Edit `server/.env`:

```env
# Database
DATABASE_URL=postgresql://user:password@localhost:5432/attendance_db

# API Authentication
API_KEY=your-api-key-here

# SMS – custom gateway (api_key, senderid, msg, numbers; schedule_date_time only for scheduled)
SMS_PROVIDER=custom
SMS_GATEWAY_URL=https://sms.mygiftcard.top/api/v1/smsapi
SMS_API_KEY=your-api-key
SMS_SENDER_ID=8809612442476

# SMS – Twilio (when SMS_PROVIDER=twilio)
TWILIO_ACCOUNT_SID=your-account-sid
TWILIO_AUTH_TOKEN=your-auth-token
TWILIO_PHONE_NUMBER=+1234567890

# Notification channel: whatsapp (Green API) or sms (custom/Twilio)
NOTIFICATION_CHANNEL=whatsapp
NOTIFICATION_FALLBACK_SMS=true

# WhatsApp (Green API) – see "Green API setup" below
GREEN_API_ID_INSTANCE=your-instance-id
GREEN_API_API_TOKEN_INSTANCE=your-api-token

# SMS (when NOTIFICATION_CHANNEL=sms)
SMS_PROVIDER=custom
SMS_GATEWAY_URL=https://sms.mygiftcard.top/api/v1/smsapi
SMS_API_KEY=your-api-key
SMS_SENDER_ID=8809612442476
```

### Green API (WhatsApp) setup and testing

Use [Green API](https://green-api.com) to send attendance notifications via WhatsApp. Both WhatsApp and custom SMS stay available; switch with `NOTIFICATION_CHANNEL`.

#### 1. Get Green API credentials

1. Go to **[green-api.com](https://green-api.com)** and sign up.
2. Create an **instance** in the dashboard (e.g. “Smart Attendance”).
3. You will get:
   - **Instance ID** (e.g. `1101234567`)
   - **API Token** (long string)
4. Link your WhatsApp to the instance:
   - In the dashboard, open your instance and use **“QR code”** or **“Link device”**.
   - Scan the QR with WhatsApp on your phone (WhatsApp Web style) so the instance can send messages.

#### 2. Configure the server

In `server/.env`:

```env
NOTIFICATION_CHANNEL=whatsapp
GREEN_API_ID_INSTANCE=1101234567
GREEN_API_API_TOKEN_INSTANCE=your-api-token-from-dashboard
```

Restart the Flask server after changing `.env`.

#### 3. Add a student with WhatsApp number

Parent number must be in international format **without** the `+` (e.g. `8801629334432` for Bangladesh). Green API uses `number@c.us` as chat ID.

```bash
curl -X POST http://localhost:5000/api/students \
  -H "Authorization: Bearer YOUR_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "epc": "E2801190200051187E26CB52",
    "student_name": "Test Student",
    "parent_name": "Parent",
    "parent_phone": "8801629334432",
    "parent_whatsapp": "8801629334432",
    "gender": "male"
  }'
```

#### 4. Test the flow

1. **Simulate a scan** (use the same EPC as the student):

   ```bash
   curl -X POST http://localhost:5000/api/attendance/scan \
     -H "Authorization: Bearer YOUR_API_KEY" \
     -H "Content-Type: application/json" \
     -d '{
       "epc": "E2801190200051187E26CB52",
       "timestamp": "2026-02-26T10:30:00",
       "rssi": -50,
       "antenna": 1
     }'
   ```

2. Check the server logs for `WhatsApp message sent successfully` or any error.
3. The parent WhatsApp number should receive:  
   `Your son Test Student entered the school at 10:30:00.`

#### 5. Switch back to SMS later

When you have `SMS_GATEWAY_URL`, set in `.env`:

```env
NOTIFICATION_CHANNEL=sms
SMS_GATEWAY_URL=https://sms.mygiftcard.top/api/v1/smsapi
SMS_API_KEY=...
SMS_SENDER_ID=8809612442476
```

No code changes needed; both systems remain in place and are selected by `NOTIFICATION_CHANNEL`.

### API Endpoints

#### POST /api/attendance/scan
Receive EPC scan from ESP32.

**Headers:**
- `Authorization: Bearer <api-key>`
- `Content-Type: application/json`

**Request:**
```json
{
  "epc": "ABCD00193409009905601234",
  "timestamp": "2026-02-13T10:30:45",
  "rssi": 130,
  "antenna": 1
}
```

#### GET /api/students
List all students (requires API key).

#### POST /api/students
Create new student (requires API key).

**Request:**
```json
{
  "epc": "ABCD00193409009905601234",
  "student_name": "John Doe",
  "parent_name": "Jane Doe",
  "parent_phone": "+1234567890",
  "parent_whatsapp": "+1234567890",
  "gender": "male"
}
```

## Usage Workflow

1. **Add students to database:**
   ```bash
   curl -X POST http://localhost:5000/api/students \
     -H "Authorization: Bearer your-api-key" \
     -H "Content-Type: application/json" \
     -d '{
       "epc": "ABCD00193409009905601234",
       "student_name": "John Doe",
       "parent_phone": "+1234567890",
       "gender": "male"
     }'
   ```

2. **ESP32 scans RFID tag:**
   - Device automatically sends scan data to server
   - Server looks up student by EPC
   - Notification sent to parent (WhatsApp or SMS)

3. **Parent receives message:**
   ```
   Your son John Doe entered the school at 10:30:45.
   ```

## Features

### ESP32 Firmware
- ✅ WiFi connection management
- ✅ HTTP client with retry logic
- ✅ EPC scan debouncing
- ✅ SNTP time synchronization
- ✅ Continuous scanning mode
- ✅ Clean serial output

### Server
- ✅ PostgreSQL database with SQLAlchemy ORM
- ✅ RESTful API endpoints
- ✅ SMS integration (Twilio)
- ✅ WhatsApp integration (Green API)
- ✅ Notification cooldown (prevents spam)
- ✅ Automatic fallback (WhatsApp → SMS)
- ✅ API key authentication
- ✅ Comprehensive error handling

## Testing

### Test ESP32 Connection
```bash
# Monitor serial output
idf.py monitor

# Check WiFi connection
# Check HTTP POST requests in server logs
```

### Test Server API
```bash
# Health check
curl http://localhost:5000/api/health

# Test scan endpoint
curl -X POST http://localhost:5000/api/attendance/scan \
  -H "Authorization: Bearer your-api-key" \
  -H "Content-Type: application/json" \
  -d '{
    "epc": "ABCD00193409009905601234",
    "timestamp": "2026-02-13T10:30:45",
    "rssi": 130,
    "antenna": 1
  }'
```

## Troubleshooting

### ESP32 Issues

**WiFi not connecting:**
- Check SSID and password in `main/config.h`
- Verify WiFi signal strength
- Check serial monitor for error messages

**HTTP requests failing:**
- Verify server URL in `main/config.h`
- Check server is running and accessible
- Verify API key matches server configuration

### Server Issues

**Database connection errors:**
- Verify PostgreSQL is running
- Check `DATABASE_URL` in `.env`
- Ensure database exists

**Notifications not sending:**
- Check API credentials in `.env`
- Verify phone numbers are in correct format (E.164)
- Check server logs for error messages

## Security Notes

- Change default API keys in production
- Use HTTPS in production (configure TLS certificates)
- Secure database credentials
- Implement rate limiting for API endpoints
- Validate all input data

## License

[Your License Here]

## Contributing

[Contributing Guidelines]
