import requests
import logging
from flask import current_app

logger = logging.getLogger(__name__)

class WhatsAppService:
    """WhatsApp service for sending notifications via Green API (free tier)"""
    
    @staticmethod
    def send_message(phone_number: str, message: str):
        """
        Send WhatsApp message via Green API
        
        Args:
            phone_number: Recipient phone number (with country code, no +)
            message: Message text
            
        Returns:
            Tuple of (success: bool, error_message: str)
        """
        id_instance = current_app.config.get('GREEN_API_ID_INSTANCE')
        api_token = current_app.config.get('GREEN_API_API_TOKEN_INSTANCE')
        base_url = current_app.config.get('GREEN_API_BASE_URL', 'https://api.green-api.com')
        
        if not all([id_instance, api_token]):
            error_msg = "Green API credentials not configured"
            logger.error(error_msg)
            return False, error_msg
        
        # Remove + from phone number if present
        phone_number = phone_number.lstrip('+')
        
        # Green API endpoint
        url = f"{base_url}/waInstance{id_instance}/sendMessage/{api_token}"
        
        payload = {
            "chatId": f"{phone_number}@c.us",
            "message": message
        }
        
        try:
            response = requests.post(
                url,
                json=payload,
                timeout=10
            )
            
            if response.status_code == 200:
                result = response.json()
                if result.get('idMessage'):
                    logger.info(f"WhatsApp message sent successfully to {phone_number}")
                    return True, ""
                else:
                    error_msg = f"Green API error: {result}"
                    logger.error(error_msg)
                    return False, error_msg
            else:
                error_msg = f"Green API HTTP error: {response.status_code} - {response.text}"
                logger.error(error_msg)
                return False, error_msg
                
        except requests.exceptions.RequestException as e:
            error_msg = f"WhatsApp send failed: {str(e)}"
            logger.error(error_msg)
            return False, error_msg
    
    @staticmethod
    def format_message(student_name: str, gender: str, timestamp: str) -> str:
        """
        Format attendance notification message (same format as SMS)
        
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
