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
            phone_number: Recipient phone number (e.g. 01629334432 or +8801629334432)
            message: Message text

        Returns:
            Tuple of (success: bool, error_message: str)
        """
        provider = current_app.config.get('SMS_PROVIDER', 'custom').lower()

        if provider == 'custom':
            return SMSService._send_via_custom_gateway(phone_number, message)
        if provider == 'twilio':
            return SMSService._send_via_twilio(phone_number, message)
        error_msg = f"Unsupported SMS provider: {provider}"
        logger.error(error_msg)
        return False, error_msg

    @staticmethod
    def _send_via_custom_gateway(phone_number: str, message: str):
        """
        Send SMS via custom gateway API.
        Request body format: api_key, type, senderid, msg, numbers [, schedule_date_time]
        schedule_date_time is only included for scheduled SMS (YYYY-MM-DD HH:MM:SS).
        """
        url = current_app.config.get('SMS_GATEWAY_URL')
        api_key = current_app.config.get('SMS_API_KEY')
        sender_id = current_app.config.get('SMS_SENDER_ID')

        if not all([url, api_key, sender_id]):
            error_msg = "Custom SMS gateway not configured: set SMS_GATEWAY_URL, SMS_API_KEY, SMS_SENDER_ID"
            logger.error(error_msg)
            return False, error_msg

        # Normalize number: strip spaces; API expects comma-separated for multiple
        numbers = phone_number.replace(' ', '').strip()

        payload = {
            "api_key": api_key,
            "type": "text",
            "senderid": sender_id,
            "msg": message,
            "numbers": numbers,
        }
        # schedule_date_time is mandatory only for scheduled SMS; omit for immediate send

        try:
            response = requests.post(
                url,
                json=payload,
                headers={"Content-Type": "application/json"},
                timeout=15,
            )
            # Gateway returns JSON with error flag; success = error is false (not just HTTP 200)
            try:
                body = response.json()
            except (ValueError, TypeError):
                body = None
            if body is not None:
                if body.get("error") is False:
                    message_id = body.get("message_id", "")
                    logger.info("SMS sent successfully to %s%s", numbers, f" (message_id={message_id})" if message_id else "")
                    return True, ""
                if body.get("error") is True:
                    error_msg = body.get("error_message", response.text) or "Unknown error"
                    error_code = body.get("error_code")
                    if error_code is not None:
                        logger.error("SMS gateway error: %s (code=%s)", error_msg, error_code)
                    else:
                        logger.error("SMS gateway error: %s", error_msg)
                    return False, error_msg
            # Non-JSON or unexpected shape: fall back to status code
            if response.status_code in (200, 201):
                logger.info("SMS sent successfully to %s", numbers)
                return True, ""
            error_msg = f"SMS gateway error: %d - %s" % (response.status_code, response.text)
            logger.error(error_msg)
            return False, error_msg
        except requests.exceptions.RequestException as e:
            error_msg = "SMS send failed: %s" % (e,)
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
