Smart Attendance System — Presentation Outline

Project Workflow Summary (from inspection)

The system is an IoT attendance tracker with this end-to-end flow:





ESP32-C6 runs firmware that: initializes NVS → WiFi → HTTP client → SNTP → UART to UHF reader; then in a loop: performs inventory scan via UHF driver → gets EPC/RSSI/antenna → debounces EPC → builds JSON → POSTs to Flask server.



Flask server receives POST at /api/attendance/scan, validates API key and JSON, looks up Student by EPC in PostgreSQL, creates AttendanceLog, checks notification cooldown, formats message, tries WhatsApp (Green API) then SMS (Twilio) fallback, updates log, returns JSON.



Admin can add students via POST /api/students (EPC, name, parent phone/WhatsApp, gender) and view logs via GET /api/attendance/logs.

Key files: main/main.cpp (scan loop, debounce, send), main/http_client.cpp (JSON POST), server/routes.py (scan + student + logs), server/models.py (Student, AttendanceLog), server/services/whatsapp_service.py, server/services/sms_service.py.



Presentation slide outline

Slide layout assumes: title area (top), main content (center), optional footnote/bullet area (bottom). Specify “Position” as: Top / Center / Bottom and Left / Center / Right where relevant.



Slide 1: Title





Title (center): Smart Attendance System  



Subtitle (center, below title): IoT-based attendance tracking with RFID and parent notifications  



Footer (bottom center): ESP32-C6 • UHF RFID • Flask • PostgreSQL • SMS/WhatsApp



Slide 2: System architecture (high-level)





Title (top): System Architecture  



Center: Use the diagram from README (or a simplified version):





ESP32-C6 (Scanner) → WiFi → Flask Server → PostgreSQL



From Flask: arrows to SMS Gateway (Twilio) and WhatsApp (Green API)



Bottom: One line: “Single flow: scan at gate → server → DB → parent notification.”

Optional: same diagram in Mermaid for the slide deck source:

flowchart LR
  ESP32[ESP32-C6 Scanner]
  WiFi[WiFi]
  Flask[Flask Server]
  DB[(PostgreSQL)]
  SMS[SMS Twilio]
  WA[WhatsApp Green API]
  ESP32 --> WiFi --> Flask --> DB
  Flask --> SMS
  Flask --> WA



Slide 3: Hardware and components





Title (top): Hardware and Components  



Left column:  





ESP32-C6 development board  



J4210U UHF RFID reader module  



UART (e.g. 57600 baud) between ESP32 and reader



Right column:  





WiFi access point  



RFID tags (EPC) for each student



Bottom: “Each student has a unique EPC; reader reports EPC, RSSI, antenna.”



Slide 4: Project structure





Title (top): Project Structure  



Center: Tree or list:





main/ — ESP32-C6 firmware: main.cpp, wifi_manager, http_client, config.h



server/ — Flask: app.py, models.py, routes.py, config.py, services/ (SMS, WhatsApp), database/



components/UHF-driver/ — UHF RFID driver (driver, 903/910, HAL serial)



Bottom: “Firmware in C/C++ (ESP-IDF); server in Python.”



Slide 5: ESP32 firmware — boot and init





Title (top): ESP32 Firmware — Startup  



Center (numbered list):  





NVS init (for WiFi)



WiFi manager init and connect (SSID/password from config.h)



HTTP client init



SNTP init (e.g. pool.ntp.org) — wait for time sync



Open UART to reader, GetSettings/SetSettings (e.g. ScanTime 300 ms, BeepOn)



Enter main scan loop



Bottom: Ref: main/main.cpp app_main().



Slide 6: ESP32 firmware — scan loop





Title (top): ESP32 — Scan Loop  



Center:  





Every cycle: optional WiFi reconnect check → Inventory(false) (UHF driver) → get tag count.  



For each tag: GetResult() → EPC bytes → convert to hex string; read RSSI, antenna.  



Debounce: same EPC within SCAN_DEBOUNCE_SECONDS (e.g. 5 s) → skip send.  



If not debounced: build scan_data_t (EPC, timestamp ISO, rssi, antenna) → http_client_send_scan_data() → POST to SERVER_URL + SERVER_ENDPOINT.  



Delay 3 s (continuous mode), repeat.



Bottom: “Timestamp from SNTP; JSON built in http_client.cpp.”



Slide 7: Data sent from ESP32 to server





Title (top): Scan Payload (ESP32 → Server)  



Center: Show the exact JSON and headers:





Endpoint: POST /api/attendance/scan  



Header: Authorization: Bearer <API_KEY>  



Body: {"epc": "ABCD...", "timestamp": "2026-02-13T10:30:45", "rssi": 130, "antenna": 1}



Bottom: “API key and URL in main/config.h; server URL in server/.env / config.”



Slide 8: Server — receiving a scan





Title (top): Server — Handling a Scan  



Center (flow):  





POST /api/attendance/scan → require_api_key → parse JSON, validate epc.



Look up Student by epc (Student.query.filter_by(epc=epc)).



If no student: still create AttendanceLog (with student_id nullable or link to “unknown”), log error, return 404.



If student: check cooldown (e.g. same student, notification_sent in last N hours).



Create AttendanceLog (student_id, epc, scan_timestamp, rssi, antenna).



If not in cooldown: format message (e.g. “Your son/daughter &lt;name&gt; entered at &lt;time&gt;”), then send notification (next slide).



Update log (notification_sent, notification_method), commit, return 200.



Bottom: Ref: server/routes.py scan_attendance().



Slide 9: Server — notifications





Title (top): Parent Notification Logic  



Center:  





Message: “Your son/daughter &lt;name&gt; entered the school at &lt;time&gt;.” (from SMSService.format_message; son/daughter by gender).  



Prefer WhatsApp if configured: WhatsAppService.send_message(parent_whatsapp, message) (Green API).  



If WhatsApp fails and parent_phone set: fallback SMSService.send_sms(parent_phone, message) (Twilio).  



Cooldown prevents duplicate notifications for same student within N hours.



Bottom: “Credentials in server/.env (Green API, Twilio).”



Slide 10: Database models





Title (top): Database Models (PostgreSQL)  



Left: Student: id, epc (unique), student_name, parent_name, parent_phone, parent_whatsapp, gender, created_at, updated_at.  



Right: AttendanceLog: id, student_id (FK), epc, scan_timestamp, rssi, antenna, notification_sent, notification_method, notification_error, created_at.  



Bottom: “ORM: SQLAlchemy; init: python database/init_db.py [--seed].”



Slide 11: API overview





Title (top): API Endpoints  



Center (table or list):  





GET /api/health — no auth; server health.  



POST /api/attendance/scan — Bearer API key; receive scan (JSON above).  



GET /api/students — Bearer; list students.  



POST /api/students — Bearer; create student (epc, student_name, parent_phone, optional parent_name, parent_whatsapp, gender).  



GET /api/attendance/logs — Bearer; list logs (optional query: limit, offset, epc).



Bottom: “All except /api/health require Authorization: Bearer <API_KEY>.”



Slide 12: End-to-end user flow





Title (top): End-to-End User Flow  



Center (numbered):  





Admin adds students (POST /api/students) with EPC and parent contact.



Student wears/brings RFID tag; passes by reader at school.



ESP32 scans tag → debounce → POST scan to server.



Server finds student, logs attendance, sends WhatsApp or SMS to parent.



Parent receives: “Your son/daughter &lt;name&gt; entered the school at &lt;time&gt;.”



Bottom: “Optional: show a single timeline graphic (student → reader → ESP32 → cloud → parent).”



Slide 13: Configuration summary





Title (top): Configuration Summary  



Left: ESP32 (main/config.h): WIFI_SSID, WIFI_PASSWORD, SERVER_URL, SERVER_ENDPOINT, API_KEY, SCAN_DEBOUNCE_SECONDS.  



Right: Server (.env): DATABASE_URL, API_KEY, Twilio (SID, token, from number), Green API (instance id, token); optional NOTIFICATION_COOLDOWN_HOURS, PREFER_WHATSAPP.  



Bottom: “API_KEY must match on ESP32 and server.”



Slide 14: Features checklist





Title (top): Features  



Two columns:  





Firmware: WiFi + retry, HTTP client + retry, EPC debounce, SNTP time, continuous scanning.  



Server: PostgreSQL + SQLAlchemy, REST API, API key auth, SMS (Twilio), WhatsApp (Green API), cooldown, WhatsApp→SMS fallback, error handling and logging.



Bottom: “Security: use HTTPS and strong API keys in production.”



Slide 15: Summary / Q&A





Title (top): Summary  



Center:  





Smart Attendance = RFID scan at gate → ESP32 → Flask → DB + parent notification (WhatsApp/SMS).  



One sentence each: hardware (ESP32 + UHF reader), firmware (scan + debounce + HTTP POST), server (lookup + log + notify).



Bottom: “Questions?” or “Thank you.”



Deliverable





Inspection: Completed; workflow is as above and matches README.md and the codebase.  



Deliverable for you: This outline is the slide-by-slide script. Each slide has title, main content, and footer/bottom with positions (top/center/bottom, left/right where needed). You can drop this into PowerPoint, Google Slides, or any slide tool and place the text/diagrams accordingly.  



Optional: Add a single “End-to-end flow” diagram on Slide 12 (student → reader → ESP32 → WiFi → Flask → DB + notifications → parent) for impact.

No code or file changes are required for this plan; it is documentation and presentation outline only.