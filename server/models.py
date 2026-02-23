from flask_sqlalchemy import SQLAlchemy
from datetime import datetime

db = SQLAlchemy()

class Student(db.Model):
    """Student model - stores student information with EPC as unique identifier"""
    __tablename__ = 'students'
    
    id = db.Column(db.Integer, primary_key=True)
    epc = db.Column(db.String(64), unique=True, nullable=False, index=True)
    student_name = db.Column(db.String(100), nullable=False)
    parent_name = db.Column(db.String(100))
    parent_phone = db.Column(db.String(20), nullable=False)  # SMS number
    parent_whatsapp = db.Column(db.String(20))  # WhatsApp number (optional)
    gender = db.Column(db.String(10))  # 'male' or 'female' for "son/daughter"
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    # Relationship
    attendance_logs = db.relationship('AttendanceLog', backref='student', lazy=True)
    
    def __repr__(self):
        return f'<Student {self.student_name} (EPC: {self.epc})>'
    
    def to_dict(self):
        return {
            'id': self.id,
            'epc': self.epc,
            'student_name': self.student_name,
            'parent_name': self.parent_name,
            'parent_phone': self.parent_phone,
            'parent_whatsapp': self.parent_whatsapp,
            'gender': self.gender
        }


class AttendanceLog(db.Model):
    """Attendance log model - stores each scan event"""
    __tablename__ = 'attendance_logs'
    
    id = db.Column(db.Integer, primary_key=True)
    student_id = db.Column(db.Integer, db.ForeignKey('students.id'), nullable=False, index=True)
    epc = db.Column(db.String(64), nullable=False, index=True)  # Redundant but useful for queries
    scan_timestamp = db.Column(db.DateTime, nullable=False, index=True)
    rssi = db.Column(db.Integer)
    antenna = db.Column(db.Integer)
    notification_sent = db.Column(db.Boolean, default=False)
    notification_method = db.Column(db.String(20))  # 'sms' or 'whatsapp'
    notification_error = db.Column(db.Text)  # Store error message if notification failed
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    
    def __repr__(self):
        return f'<AttendanceLog {self.epc} at {self.scan_timestamp}>'
    
    def to_dict(self):
        return {
            'id': self.id,
            'student_id': self.student_id,
            'epc': self.epc,
            'scan_timestamp': self.scan_timestamp.isoformat(),
            'rssi': self.rssi,
            'antenna': self.antenna,
            'notification_sent': self.notification_sent,
            'notification_method': self.notification_method
        }
