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
    SMS_PROVIDER = os.getenv('SMS_PROVIDER', 'twilio')
    TWILIO_ACCOUNT_SID = os.getenv('TWILIO_ACCOUNT_SID', '')
    TWILIO_AUTH_TOKEN = os.getenv('TWILIO_AUTH_TOKEN', '')
    TWILIO_PHONE_NUMBER = os.getenv('TWILIO_PHONE_NUMBER', '')
    
    # WhatsApp Configuration (Green API)
    WHATSAPP_PROVIDER = os.getenv('WHATSAPP_PROVIDER', 'green_api')
    GREEN_API_ID_INSTANCE = os.getenv('GREEN_API_ID_INSTANCE', '')
    GREEN_API_API_TOKEN_INSTANCE = os.getenv('GREEN_API_API_TOKEN_INSTANCE', '')
    GREEN_API_BASE_URL = 'https://api.green-api.com'
    
    # Notification Settings
    NOTIFICATION_COOLDOWN_HOURS = int(os.getenv('NOTIFICATION_COOLDOWN_HOURS', 1))
    PREFER_WHATSAPP = os.getenv('PREFER_WHATSAPP', 'true').lower() == 'true'
