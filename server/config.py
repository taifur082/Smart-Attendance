import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    """Application configuration"""
    
    # Database
    SQLALCHEMY_DATABASE_URI = os.getenv('DATABASE_URL', 'postgresql://localhost/attendance_db')
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    
    # Flask
    SECRET_KEY = os.getenv('SECRET_KEY', 'dev-secret-key-change-in-production')
    
    # Server
    HOST = os.getenv('SERVER_HOST', '0.0.0.0')
    PORT = int(os.getenv('SERVER_PORT', 5000))
    
    # API Authentication
    API_KEY = os.getenv('API_KEY', '')
    
    # SMS Configuration
    SMS_PROVIDER = os.getenv('SMS_PROVIDER', 'custom').lower()
    # Custom SMS gateway (JSON: api_key, type, senderid, msg, numbers[, schedule_date_time])
    SMS_GATEWAY_URL = os.getenv('SMS_GATEWAY_URL', 'https://sms.mygiftcard.top/api/v1/smsapi')
    SMS_API_KEY = os.getenv('SMS_API_KEY', '')
    SMS_SENDER_ID = os.getenv('SMS_SENDER_ID', '')     # e.g. 8809612442476
    # Twilio (when SMS_PROVIDER=twilio)
    TWILIO_ACCOUNT_SID = os.getenv('TWILIO_ACCOUNT_SID', '')
    TWILIO_AUTH_TOKEN = os.getenv('TWILIO_AUTH_TOKEN', '')
    TWILIO_PHONE_NUMBER = os.getenv('TWILIO_PHONE_NUMBER', '')
    
    # WhatsApp Configuration (Green API)
    WHATSAPP_PROVIDER = os.getenv('WHATSAPP_PROVIDER', 'green_api')
    GREEN_API_ID_INSTANCE = os.getenv('GREEN_API_ID_INSTANCE', '')
    GREEN_API_API_TOKEN_INSTANCE = os.getenv('GREEN_API_API_TOKEN_INSTANCE', '')
    GREEN_API_BASE_URL = 'https://7103.api.greenapi.com'
    
    # Notification channel: which API to use for attendance notifications
    # 'whatsapp' = Green API (WhatsApp); 'sms' = custom SMS gateway (or Twilio)
    NOTIFICATION_CHANNEL = os.getenv('NOTIFICATION_CHANNEL', 'whatsapp').lower()
    NOTIFICATION_COOLDOWN_HOURS = int(os.getenv('NOTIFICATION_COOLDOWN_HOURS', 1))
    # When channel is 'whatsapp' and send fails, try SMS if student has parent_phone
    NOTIFICATION_FALLBACK_SMS = os.getenv('NOTIFICATION_FALLBACK_SMS', 'true').lower() == 'true'
