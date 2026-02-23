# Smart Attendance System - Server

Flask server for receiving EPC scan data and sending SMS/WhatsApp notifications.

## Setup

1. **Install dependencies:**
```bash
pip install -r requirements.txt
```

2. **Configure environment:**
```bash
cp .env.example .env
# Edit .env with your configuration
```

3. **Create PostgreSQL database:**
```sql
CREATE DATABASE attendance_db;
```

4. **Initialize database:**
```bash
python database/init_db.py
# Or with test data:
python database/init_db.py --seed
```

5. **Run server:**
```bash
python app.py
```

## API Endpoints

### POST /api/attendance/scan
Receive EPC scan data from ESP32.

**Headers:**
- `Authorization: Bearer <api-key>`
- `Content-Type: application/json`

**Request Body:**
```json
{
  "epc": "ABCD00193409009905601234",
  "timestamp": "2026-02-13T10:30:45",
  "rssi": 130,
  "antenna": 1
}
```

**Response:**
```json
{
  "success": true,
  "student": {
    "id": 1,
    "epc": "ABCD00193409009905601234",
    "student_name": "John Doe",
    ...
  },
  "notification_sent": true,
  "notification_method": "whatsapp"
}
```

### GET /api/health
Health check endpoint.

### GET /api/students
Get all students (requires API key).

### POST /api/students
Create a new student (requires API key).

### GET /api/attendance/logs
Get attendance logs (requires API key).

## Configuration

See `.env.example` for all configuration options.

### SMS Configuration (Twilio)
- Sign up at https://www.twilio.com
- Get Account SID, Auth Token, and Phone Number
- Add to `.env` file

### WhatsApp Configuration (Green API)
- Sign up at https://green-api.com
- Create WhatsApp Business instance
- Get Instance ID and API Token
- Add to `.env` file

## Notes

- Green API free tier: 100 messages/day
- Notifications have a cooldown period (default: 1 hour) to prevent spam
- System prefers WhatsApp if available, falls back to SMS
