"""One-time seed: add demo students for EPCs detected by device."""
from app import create_app
from models import db, Student, TAG_TYPE_UHF, TAG_TYPE_RC522

DEMO_STUDENTS = [
    {"tag_type": TAG_TYPE_UHF, "epc": "ABCD00193409009905601234", "student_name": "Borno", "parent_name": "Shakila",
     "parent_phone": "8801631518835", "parent_whatsapp": "8801631518835", "gender": "female"},
    {"tag_type": TAG_TYPE_UHF, "epc": "300833B2DDD901400000E280", "student_name": "Fariatul Islam Nuha", "parent_name": "Demo Parent 2",
     "parent_phone": "8801641898813", "parent_whatsapp": "8801866092790", "gender": "female"},
    {"tag_type": TAG_TYPE_UHF, "epc": "E2002083980B01320390E6EB", "student_name": "Amit Kumar Bor", "parent_name": "Demo Parent 3",
     "parent_phone": "8801641898813", "parent_whatsapp": "8801641900883", "gender": "male"},
    # RC522 MIFARE card (UID from scanner log)
    {"tag_type": TAG_TYPE_RC522, "epc": "0E1E2902", "student_name": "RC522 Demo Student", "parent_name": "Demo Parent",
     "parent_phone": "8801641898813", "parent_whatsapp": "8801641898813", "gender": "male"},
]

def seed():
    app = create_app()
    with app.app_context():
        for data in DEMO_STUDENTS:
            if Student.query.filter_by(tag_type=data["tag_type"], epc=data["epc"]).first():
                print(f"Skip (exists): {data['epc']}")
                continue
            db.session.add(Student(**data))
        db.session.commit()
        print("Demo students added.")

if __name__ == "__main__":
    seed()