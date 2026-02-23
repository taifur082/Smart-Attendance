import requests
import logging
from flask import current_app

logger = logging.getLogger(__name__)

class SMSService:
    """SMS service for sending notifications via SMS gateway"""
    
    @staticmethod
    def send_sms(phone_number: str, message: str):
        """
        Send SMS message
        
        Args:
            phone_number: Recipient phone number (E.164 format)
            message: Message text
            
        Returns:
            Tuple of (success: bool, error_message: str)
        """
        provider = current_app.config.get('SMS_PROVIDER', 'twilio').lower()
        
        if provider == 'twilio':
            return SMSService._send_via_twilio(phone_number, message)
        else:
            error_msg = f"Unsupported SMS provider: {provider}"
            logger.error(error_msg)
            return False, error_msg
    
    @staticmethod
    def _send_via_twilio(phone_number: str, message: str):
        """Send SMS via Twilio API"""
        account_sid = current_app.config.get('TWILIO_ACCOUNT_SID')
        auth_token = current_app.config.get('TWILIO_AUTH_TOKEN')
        from_number = current_app.config.get('TWILIO_PHONE_NUMBER')
        
        if not all([account_sid, auth_token, from_number]):
            error_msg = "Twilio credentials not configured"
            logger.error(error_msg)
            return False, error_msg
        
        url = f"https://api.twilio.com/2010-04-01/Accounts/{account_sid}/Messages.json"
        
        data = {
            'From': from_number,
            'To': phone_number,
            'Body': message
        }
        
        try:
            response = requests.post(
                url,
                auth=(account_sid, auth_token),
                data=data,
                timeout=10
            )
            
            if response.status_code == 201:
                logger.info(f"SMS sent successfully to {phone_number}")
                return True, ""
            else:
                error_msg = f"Twilio API error: {response.status_code} - {response.text}"
                logger.error(error_msg)
                return False, error_msg
                
        except requests.exceptions.RequestException as e:
            error_msg = f"SMS send failed: {str(e)}"
            logger.error(error_msg)
            return False, error_msg
    
    @staticmethod
    def format_message(student_name: str, gender: str, timestamp: str) -> str:
        """
        Format attendance notification message
        
        Args:
            student_name: Name of the student
            gender: 'male' or 'female' (for "son/daughter")
            timestamp: ISO format timestamp string
            
        Returns:
            Formatted message string
        """
        # Extract time from timestamp (format: 2026-02-13T10:30:45)
        try:
            time_str = timestamp.split('T')[1].split('.')[0] if 'T' in timestamp else timestamp
        except:
            time_str = timestamp
        
        relationship = "son" if gender and gender.lower() == 'male' else "daughter"
        
        message = f"Your {relationship} {student_name} entered the school at {time_str}."
        return message
