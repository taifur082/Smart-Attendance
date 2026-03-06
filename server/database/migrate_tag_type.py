"""
One-time migration: add tag_type to students and attendance_logs,
and replace unique(epc) with unique(tag_type, epc) on students.
Run with: python -m database.migrate_tag_type
"""
from app import create_app
from models import db
from sqlalchemy import text
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def migrate():
    app = create_app()
    with app.app_context():
        # Detect driver (postgresql vs sqlite)
        url = db.engine.url
        is_pg = url.drivername.startswith("postgresql")

        if is_pg:
            # PostgreSQL
            # Add tag_type columns with default 'uhf'
            try:
                db.session.execute(text("""
                    ALTER TABLE students
                    ADD COLUMN IF NOT EXISTS tag_type VARCHAR(10) NOT NULL DEFAULT 'uhf'
                """))
                db.session.execute(text("""
                    ALTER TABLE attendance_logs
                    ADD COLUMN IF NOT EXISTS tag_type VARCHAR(10) NOT NULL DEFAULT 'uhf'
                """))
                db.session.commit()
            except Exception as e:
                logger.warning("Adding columns (may already exist): %s", e)
                db.session.rollback()

            # Drop old unique constraint on students.epc (PostgreSQL names it students_epc_key)
            try:
                db.session.execute(text("ALTER TABLE students DROP CONSTRAINT IF EXISTS students_epc_key"))
                db.session.commit()
            except Exception as e:
                logger.warning("Dropping old constraint: %s", e)
                db.session.rollback()

            # Add new unique constraint on (tag_type, epc)
            try:
                db.session.execute(text("""
                    ALTER TABLE students
                    ADD CONSTRAINT uq_student_tag_type_epc UNIQUE (tag_type, epc)
                """))
                db.session.commit()
            except Exception as e:
                logger.warning("Adding new constraint (may already exist): %s", e)
                db.session.rollback()

            # Make attendance_logs.student_id nullable
            try:
                db.session.execute(text("""
                    ALTER TABLE attendance_logs
                    ALTER COLUMN student_id DROP NOT NULL
                """))
                db.session.commit()
            except Exception as e:
                logger.warning("Altering student_id: %s", e)
                db.session.rollback()
        else:
            # SQLite (e.g. tests)
            try:
                db.session.execute(text("""
                    ALTER TABLE students ADD COLUMN tag_type VARCHAR(10) DEFAULT 'uhf' NOT NULL
                """))
                db.session.commit()
            except Exception as e:
                logger.warning("SQLite add column students: %s", e)
                db.session.rollback()
            try:
                db.session.execute(text("""
                    ALTER TABLE attendance_logs ADD COLUMN tag_type VARCHAR(10) DEFAULT 'uhf' NOT NULL
                """))
                db.session.commit()
            except Exception as e:
                logger.warning("SQLite add column attendance_logs: %s", e)
                db.session.rollback()
            # SQLite doesn't support DROP CONSTRAINT / ADD CONSTRAINT easily; recreate table or leave as-is.
            # For SQLite, unique(epc) may remain; new code uses (tag_type,epc) in app logic.

        logger.info("Migration completed.")


if __name__ == "__main__":
    migrate()
