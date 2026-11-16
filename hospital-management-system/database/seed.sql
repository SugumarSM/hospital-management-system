-- Seed Data for Hospital Management System
PRAGMA foreign_keys = ON;

-- 1. Insert Departments
INSERT OR IGNORE INTO departments (id, department_name, description) VALUES
(1, 'Cardiology', 'Heart health, cardiovascular diseases, hypertension, and ECG/Echocardiography'),
(2, 'Neurology', 'Disorders of the nervous system, brain, stroke care, and spinal disorders'),
(3, 'Pediatrics', 'Comprehensive medical care for infants, children, and adolescents'),
(4, 'Orthopedics', 'Bones, joints, ligaments, tendons, musculoskeletal trauma and surgery'),
(5, 'General Medicine', 'Primary care, infectious diseases, adult routine health checkups, and diabetes'),
(6, 'Dermatology', 'Skin, hair, nails, allergic skin reactions, and cosmetic treatments');

-- 2. Insert Users (Admin, Doctors, Receptionist, Patients)
-- Passwords:
-- admin: admin123
-- doctors: doctor123
-- receptionist: recep123
-- patients: patient123

INSERT OR IGNORE INTO users (id, username, email, password_hash, role, status) VALUES
(1, 'admin', 'admin@hospital.com', 'sha256$e1f9a2b3c4d5e6f7$71b38a0fb4263c84f3d80502c14610c05e7264df2bd0d360769f09d27101f304', 'admin', 'active'),
(2, 'dr.smith', 'smith@hospital.com', 'sha256$e1f9a2b3c4d5e6f7$211b78191133d17cbba213775ae5fd0646e57d2efaa1803bb0236d0d34b35973', 'doctor', 'active'),
(3, 'dr.johnson', 'johnson@hospital.com', 'sha256$e1f9a2b3c4d5e6f7$211b78191133d17cbba213775ae5fd0646e57d2efaa1803bb0236d0d34b35973', 'doctor', 'active'),
(4, 'dr.williams', 'williams@hospital.com', 'sha256$e1f9a2b3c4d5e6f7$211b78191133d17cbba213775ae5fd0646e57d2efaa1803bb0236d0d34b35973', 'doctor', 'active'),
(5, 'dr.sharma', 'sharma@hospital.com', 'sha256$e1f9a2b3c4d5e6f7$211b78191133d17cbba213775ae5fd0646e57d2efaa1803bb0236d0d34b35973', 'doctor', 'active'),
(6, 'receptionist', 'receptionist@hospital.com', 'sha256$e1f9a2b3c4d5e6f7$2cde26171cde5946478ad831e61066ef18180472284313bafa97aaa98569621c', 'receptionist', 'active'),
(7, 'patient1', 'rahul.verma@example.com', 'sha256$e1f9a2b3c4d5e6f7$7238e9830202909ffcddec0b1fcd34facb0dc49754f132f49a14d85a32bdc201', 'patient', 'active'),
(8, 'patient2', 'priya.sharma@example.com', 'sha256$e1f9a2b3c4d5e6f7$7238e9830202909ffcddec0b1fcd34facb0dc49754f132f49a14d85a32bdc201', 'patient', 'active');

-- 3. Insert Doctors
INSERT OR IGNORE INTO doctors (id, user_id, doctor_code, full_name, department_id, specialization, qualification, consultation_fee, availability, status) VALUES
(1, 2, 'DOC-1001', 'Dr. John Smith', 1, 'Interventional Cardiologist', 'MBBS, MD, DM (Cardiology)', 700.0, '09:00 - 15:00', 'active'),
(2, 3, 'DOC-1002', 'Dr. Emily Johnson', 2, 'Consultant Neurologist', 'MBBS, MD (Med), DM (Neuro)', 850.0, '10:00 - 16:00', 'active'),
(3, 4, 'DOC-1003', 'Dr. Robert Williams', 3, 'Senior Pediatrician', 'MBBS, DCH, MD (Pediatrics)', 600.0, '08:30 - 14:30', 'active'),
(4, 5, 'DOC-1004', 'Dr. Ananya Sharma', 4, 'Orthopedic Surgeon', 'MBBS, MS (Orthopedics), MCh', 750.0, '11:00 - 17:00', 'active');

-- 4. Insert Patients
INSERT OR IGNORE INTO patients (id, patient_code, user_id, full_name, date_of_birth, gender, phone, address, emergency_contact, blood_group) VALUES
(1, 'PAT-1001', 7, 'Rahul Verma', '1990-05-14', 'Male', '9876543210', '42 MG Road, Bangalore', 'Sunita Verma (+91 9876543211)', 'B+'),
(2, 'PAT-1002', 8, 'Priya Sharma', '1985-08-22', 'Female', '9812345678', '15 Park Avenue, Mumbai', 'Vikram Sharma (+91 9812345679)', 'O+'),
(3, 'PAT-1003', NULL, 'Amit Patel', '1978-12-03', 'Male', '9845678901', '88 Ring Road, Delhi', 'Kavita Patel (+91 9845678902)', 'A+'),
(4, 'PAT-1004', NULL, 'Sneha Reddy', '1995-03-30', 'Female', '9765432109', '12 Jubilee Hills, Hyderabad', 'Kiran Reddy (+91 9765432108)', 'AB+');

-- 5. Insert Medicines (Pharmacy)
INSERT OR IGNORE INTO medicines (id, medicine_name, category, quantity, unit_price, expiry_date, minimum_stock) VALUES
(1, 'Paracetamol 500mg', 'Analgesic / Antipyretic', 150, 30.00, '2027-12-31', 25),
(2, 'Amoxicillin 500mg', 'Antibiotic', 85, 120.00, '2027-08-15', 20),
(3, 'Amlodipine 5mg', 'Antihypertensive', 12, 45.00, '2027-05-20', 20),
(4, 'Metformin 500mg', 'Antidiabetic', 200, 55.00, '2028-01-10', 30),
(5, 'Cetirizine 10mg', 'Antihistamine', 8, 25.00, '2026-12-31', 15),
(6, 'Omeprazole 20mg', 'Antacid / PPI', 90, 60.00, '2027-11-30', 20),
(7, 'Ibuprofen 400mg', 'NSAID', 110, 40.00, '2027-09-15', 25),
(8, 'Azithromycin 500mg', 'Antibiotic', 40, 180.00, '2027-04-10', 15),
(9, 'Pantoprazole 40mg', 'Antacid / PPI', 130, 75.00, '2027-10-25', 20),
(10, 'Atorvastatin 10mg', 'Lipid-lowering', 75, 95.00, '2027-07-12', 20);

-- 6. Insert Appointments
INSERT OR IGNORE INTO appointments (id, patient_id, doctor_id, appointment_date, appointment_time, reason, status) VALUES
(1, 1, 1, '2026-11-15', '09:30', 'Routine hypertension follow-up', 'Completed'),
(2, 2, 2, '2026-11-16', '10:30', 'Severe migraines and visual aura', 'Completed'),
(3, 3, 1, '2026-11-18', '11:00', 'Chest heaviness and shortness of breath', 'Confirmed'),
(4, 4, 3, '2026-11-18', '09:00', 'Pediatric wellness checkup and vaccination', 'Confirmed'),
(5, 1, 4, '2026-11-19', '12:00', 'Right knee joint pain after jogging', 'Pending');

-- 7. Insert Medical Records
INSERT OR IGNORE INTO medical_records (id, patient_id, doctor_id, appointment_id, diagnosis, consultation_notes) VALUES
(1, 1, 1, 1, 'Essential Hypertension - Stage 1', 'Blood pressure recorded 138/88 mmHg. Advised low sodium diet, 30 mins brisk walking. Continue current medication.'),
(2, 2, 2, 2, 'Migraine without Aura', 'Frequent unilateral pulsating headaches. Triggered by stress and irregular sleep. Recommended lifestyle adjustments and prophylactic therapy.');

-- 8. Insert Prescriptions
INSERT OR IGNORE INTO prescriptions (id, patient_id, doctor_id, medical_record_id, medicine_name, dosage, frequency, duration, instructions) VALUES
(1, 1, 1, 1, 'Amlodipine 5mg', '1 Tablet', 'Once daily (Morning)', '30 Days', 'Take with or without food. Avoid abrupt discontinuation.'),
(2, 1, 1, 1, 'Atorvastatin 10mg', '1 Tablet', 'Once daily (Bedtime)', '30 Days', 'Take at bedtime with water.'),
(3, 2, 2, 2, 'Naproxen 250mg', '1 Tablet', 'Twice daily after meals', '5 Days', 'Take only during acute migraine onset.');

-- 9. Insert Invoices & Payments
INSERT OR IGNORE INTO invoices (id, patient_id, appointment_id, subtotal, tax, total_amount, payment_status) VALUES
(1, 1, 1, 700.0, 35.0, 735.0, 'Paid'),
(2, 2, 2, 850.0, 42.5, 892.5, 'Paid'),
(3, 3, 3, 700.0, 35.0, 735.0, 'Unpaid');

INSERT OR IGNORE INTO payments (id, invoice_id, amount, payment_method, reference_number) VALUES
(1, 1, 735.0, 'UPI', 'PAY-UPI-20261115-001'),
(2, 2, 892.5, 'Card', 'PAY-CARD-20261116-002');

-- 10. Insert Emergency Cases
INSERT OR IGNORE INTO emergency_cases (id, patient_id, reported_symptoms, priority, status) VALUES
(1, 3, 'Acute crushing chest pain, diaphoresis, radiating to left arm', 'Critical', 'In-Treatment'),
(2, 4, 'High grade fever 104F, severe dehydration, lethargy', 'High', 'Waiting'),
(3, 1, 'Deep laceration on right forearm with active bleeding', 'Medium', 'Waiting');

-- 11. Insert Audit Logs
INSERT OR IGNORE INTO audit_logs (id, user_id, action, entity_type, entity_id, details) VALUES
(1, 1, 'SYSTEM_INIT', 'DATABASE', 1, 'Hospital Management System database initialized'),
(2, 1, 'SEED_DATA', 'DATABASE', 1, 'Demo users, doctors, medicines, and appointments seeded');

