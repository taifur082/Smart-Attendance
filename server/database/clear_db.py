"""
Clear/reset the attendance database.
Use this to remove dummy or old data before going live.

Usage:
  python -m database.clear_db           # Delete all rows (keeps tables)
  python -m database.clear_db --recreate # Drop all tables and recreate (full reset)
"""
from app import create_app
from models import db, Student, AttendanceLog
import logging
import sys

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def clear_data():
    """Delete all data from students and attendance_logs. Keeps table structure."""
    app = create_app()
    with app.app_context():
        deleted_logs = db.session.query(AttendanceLog).delete()
        deleted_students = db.session.query(Student).delete()
        db.session.commit()
        logger.info("Cleared %d attendance log(s) and %d student(s).", deleted_logs, deleted_students)


def recreate_tables():
    """Drop all tables and recreate them (full reset)."""
    app = create_app()
    with app.app_context():
        db.drop_all()
        logger.info("Dropped all tables.")
        db.create_all()
        logger.info("Recreated all tables.")


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--recreate':
        recreate_tables()
    else:
        clear_data()
        logger.info("Run with --recreate to drop and recreate all tables.")
