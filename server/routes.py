from flask import Blueprint, request, jsonify
from datetime import datetime, timedelta
from models import db, Student, AttendanceLog, TAG_TYPE_UHF, TAG_TYPE_RC522
from services.sms_service import SMSService
from services.whatsapp_service import WhatsAppService
import logging

logger = logging.getLogger(__name__)

api = Blueprint('api', __name__, url_prefix='/api')

def require_api_key(f):
    """Decorator to require API key authentication"""
    from functools import wraps
    from flask import current_app
    
    @wraps(f)
    def decorated_function(*args, **kwargs):
        api_key = request.headers.get('Authorization', '').replace('Bearer ', '')
        expected_key = current_app.config.get('API_KEY', '')
        
        if expected_key and api_key != expected_key:
            return jsonify({'error': 'Unauthorized'}), 401
        return f(*args, **kwargs)
    return decorated_function

@api.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({
        'status': 'ok',
        'timestamp': datetime.utcnow().isoformat()
    })

@api.route('/attendance/scan', methods=['POST'])
@require_api_key
def scan_attendance():
    """
    Receive EPC/UID scan data from ESP32 (UHF or RC522).
    
    Expected JSON:
    {
        "epc": "ABCD00193409009905601234" or "0E1E2902",
        "timestamp": "2026-02-13T10:30:45",
        "rssi": 130,
        "antenna": 1,
        "tag_type": "uhf" or "rc522"  // optional, default "uhf"
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No JSON data provided'}), 400
        
        # Validate required fields
        epc = data.get('epc')
        timestamp_str = data.get('timestamp')
        rssi = data.get('rssi', 0)
        antenna = data.get('antenna', 0)
        tag_type = (data.get('tag_type') or TAG_TYPE_UHF).strip().lower()
        if tag_type not in (TAG_TYPE_UHF, TAG_TYPE_RC522):
            tag_type = TAG_TYPE_UHF
        
        if not epc:
            return jsonify({'error': 'EPC is required'}), 400
        
        # Parse timestamp
        try:
            scan_timestamp = datetime.fromisoformat(timestamp_str.replace('Z', '+00:00'))
        except:
            scan_timestamp = datetime.utcnow()
            logger.warning(f"Invalid timestamp format, using current time: {timestamp_str}")
        
        # Lookup student by (tag_type, epc)
        student = Student.query.filter_by(tag_type=tag_type, epc=epc).first()
        
        if not student:
            logger.warning(f"Student not found for tag_type={tag_type} EPC: {epc}")
            # Still log the attendance attempt (student_id null)
            attendance_log = AttendanceLog(
                tag_type=tag_type,
                epc=epc,
                scan_timestamp=scan_timestamp,
                rssi=rssi,
                antenna=antenna,
                notification_sent=False,
                notification_error=f"Student not found for EPC: {epc}"
            )
            db.session.add(attendance_log)
            db.session.commit()
            
            return jsonify({
                'success': False,
                'error': 'Student not found',
                'epc': epc
            }), 404
        
        # Check if notification was sent recently (cooldown)
        from flask import current_app
        cooldown_hours = current_app.config.get('NOTIFICATION_COOLDOWN_HOURS', 1)
        cooldown_time = datetime.utcnow() - timedelta(hours=cooldown_hours)
        
        recent_log = AttendanceLog.query.filter(
            AttendanceLog.student_id == student.id,
            AttendanceLog.scan_timestamp >= cooldown_time,
            AttendanceLog.notification_sent == True
        ).first()
        
        # Log attendance
        attendance_log = AttendanceLog(
            student_id=student.id,
            tag_type=tag_type,
            epc=epc,
            scan_timestamp=scan_timestamp,
            rssi=rssi,
            antenna=antenna,
            notification_sent=False
        )
        db.session.add(attendance_log)
        db.session.flush()  # Get the ID
        
        # Send notification if not in cooldown
        notification_sent = False
        notification_method = None
        notification_error = None
        
        if not recent_log:
            # Format message (same format for WhatsApp and SMS)
            message = SMSService.format_message(
                student.student_name,
                student.gender,
                timestamp_str
            )
            
            # Use NOTIFICATION_CHANNEL: 'whatsapp' = Green API, 'sms' = custom/Twilio SMS
            channel = current_app.config.get('NOTIFICATION_CHANNEL', 'whatsapp').lower()
            fallback_sms = current_app.config.get('NOTIFICATION_FALLBACK_SMS', True)
            
            if channel == 'whatsapp' and student.parent_whatsapp:
                success, error = WhatsAppService.send_message(
                    student.parent_whatsapp,
                    message
                )
                if success:
                    notification_sent = True
                    notification_method = 'whatsapp'
                else:
                    notification_error = error
                    logger.warning("WhatsApp failed, %s", error)
                    if fallback_sms and student.parent_phone:
                        success, error = SMSService.send_sms(student.parent_phone, message)
                        if success:
                            notification_sent = True
                            notification_method = 'sms'
                        else:
                            notification_error = notification_error or error
            elif channel == 'whatsapp' and not student.parent_whatsapp and student.parent_phone and fallback_sms:
                # Channel is WhatsApp but no WhatsApp number → try SMS
                success, error = SMSService.send_sms(student.parent_phone, message)
                if success:
                    notification_sent = True
                    notification_method = 'sms'
                else:
                    notification_error = error
            elif channel == 'sms' and student.parent_phone:
                success, error = SMSService.send_sms(student.parent_phone, message)
                if success:
                    notification_sent = True
                    notification_method = 'sms'
                else:
                    notification_error = error
            else:
                if channel == 'whatsapp':
                    notification_error = "WhatsApp chosen but no parent_whatsapp (and no SMS fallback or parent_phone)"
                else:
                    notification_error = "SMS chosen but no parent_phone"
                logger.warning(notification_error)
            
            # Update attendance log
            attendance_log.notification_sent = notification_sent
            attendance_log.notification_method = notification_method
            attendance_log.notification_error = notification_error
            
            if notification_sent:
                logger.info(f"Notification sent via {notification_method} for student {student.student_name}")
            else:
                logger.error(f"Notification failed for student {student.student_name}: {notification_error}")
        else:
            logger.info(f"Notification skipped (cooldown) for student {student.student_name}")
        
        db.session.commit()
        
        return jsonify({
            'success': True,
            'student': student.to_dict(),
            'notification_sent': notification_sent,
            'notification_method': notification_method
        }), 200
        
    except Exception as e:
        logger.error(f"Error processing scan: {str(e)}", exc_info=True)
        db.session.rollback()
        return jsonify({'error': 'Internal server error', 'message': str(e)}), 500

@api.route('/students', methods=['GET'])
@require_api_key
def get_students():
    """Get all students (admin endpoint). Optional query: ?tag_type=uhf|rc522"""
    try:
        tag_type = request.args.get('tag_type', '').strip().lower()
        query = Student.query
        if tag_type in (TAG_TYPE_UHF, TAG_TYPE_RC522):
            query = query.filter_by(tag_type=tag_type)
        students = query.all()
        return jsonify({
            'success': True,
            'students': [s.to_dict() for s in students],
            'count': len(students)
        }), 200
    except Exception as e:
        logger.error(f"Error getting students: {str(e)}", exc_info=True)
        return jsonify({'error': 'Internal server error'}), 500

@api.route('/students', methods=['POST'])
@require_api_key
def create_student():
    """
    Create a new student (admin endpoint)
    
    Expected JSON:
    {
        "epc": "ABCD00193409009905601234" or "0E1E2902",
        "student_name": "John Doe",
        "parent_name": "Jane Doe",
        "parent_phone": "+1234567890",
        "parent_whatsapp": "+1234567890",  // optional
        "gender": "male",  // or "female"
        "tag_type": "uhf" or "rc522"  // optional, default "uhf"
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No JSON data provided'}), 400
        
        # Validate required fields
        required_fields = ['epc', 'student_name', 'parent_phone']
        for field in required_fields:
            if field not in data:
                return jsonify({'error': f'{field} is required'}), 400
        
        tag_type = (data.get('tag_type') or TAG_TYPE_UHF).strip().lower()
        if tag_type not in (TAG_TYPE_UHF, TAG_TYPE_RC522):
            tag_type = TAG_TYPE_UHF
        
        # Check if (tag_type, epc) already exists
        existing = Student.query.filter_by(tag_type=tag_type, epc=data['epc']).first()
        if existing:
            return jsonify({'error': f'Student with this tag_type and EPC already exists'}), 409
        
        # Create student
        student = Student(
            tag_type=tag_type,
            epc=data['epc'],
            student_name=data['student_name'],
            parent_name=data.get('parent_name'),
            parent_phone=data['parent_phone'],
            parent_whatsapp=data.get('parent_whatsapp'),
            gender=data.get('gender', 'male')
        )
        
        db.session.add(student)
        db.session.commit()
        
        logger.info(f"Created student: {student.student_name} (EPC: {student.epc})")
        
        return jsonify({
            'success': True,
            'student': student.to_dict()
        }), 201
        
    except Exception as e:
        logger.error(f"Error creating student: {str(e)}", exc_info=True)
        db.session.rollback()
        return jsonify({'error': 'Internal server error', 'message': str(e)}), 500

@api.route('/attendance/logs', methods=['GET'])
@require_api_key
def get_attendance_logs():
    """Get attendance logs (admin endpoint). Optional query: ?tag_type=uhf|rc522&epc=..."""
    try:
        limit = request.args.get('limit', 100, type=int)
        offset = request.args.get('offset', 0, type=int)
        epc = request.args.get('epc')
        tag_type = request.args.get('tag_type', '').strip().lower()
        
        query = AttendanceLog.query
        
        if tag_type in (TAG_TYPE_UHF, TAG_TYPE_RC522):
            query = query.filter_by(tag_type=tag_type)
        if epc:
            query = query.filter_by(epc=epc)
        
        query = query.order_by(AttendanceLog.scan_timestamp.desc())
        logs = query.limit(limit).offset(offset).all()
        
        return jsonify({
            'success': True,
            'logs': [log.to_dict() for log in logs],
            'count': len(logs)
        }), 200
    except Exception as e:
        logger.error(f"Error getting attendance logs: {str(e)}", exc_info=True)
        return jsonify({'error': 'Internal server error'}), 500
