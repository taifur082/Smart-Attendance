"""
Database initialization script
Run this to create database tables and optionally seed with test data
"""
from app import create_app
from models import db, Student
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def init_database():
    """Initialize database tables"""
    app = create_app()
    with app.app_context():
        db.create_all()
        logger.info("Database tables created successfully")

def seed_test_data():
    """Seed database with test student data"""
    app = create_app()
    with app.app_context():
        # Check if data already exists
        if Student.query.count() > 0:
            logger.info("Database already contains data, skipping seed")
            return
        
        # Add test students
        test_students = [
            {
                'epc': 'ABCD00193409009905601234',
                'student_name': 'John Doe',
                'parent_name': 'Jane Doe',
                'parent_phone': '+1234567890',
                'parent_whatsapp': '+1234567890',
                'gender': 'male'
            },
            {
                'epc': 'E2801190200051187E26CB52',
                'student_name': 'Alice Smith',
                'parent_name': 'Bob Smith',
                'parent_phone': '+0987654321',
                'parent_whatsapp': '+0987654321',
                'gender': 'female'
            }
        ]
        
        for student_data in test_students:
            student = Student(**student_data)
            db.session.add(student)
        
        db.session.commit()
        logger.info(f"Seeded {len(test_students)} test students")

if __name__ == '__main__':
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == '--seed':
        init_database()
        seed_test_data()
    else:
        init_database()
        logger.info("Run with --seed to add test data")
