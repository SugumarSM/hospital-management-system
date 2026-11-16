#!/usr/bin/env python3
"""
Comprehensive Automated Test Suite for Hospital Management System (C Backend)
Tests all 16 test requirements specified in Section 13.
"""

import os
import sys
import json
import subprocess
import unittest

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE_PATH = os.path.join(BASE_DIR, "build", "hospital_api.exe" if os.name == "nt" else "hospital_api.cgi")
DB_PATH = os.path.join(BASE_DIR, "database", "hospital.db")

def cgi_request(method, path, body=None, token=None, query=""):
    env = dict(os.environ)
    env["REQUEST_METHOD"] = method
    env["PATH_INFO"] = path
    env["QUERY_STRING"] = query
    env["HOSPITAL_DB_PATH"] = DB_PATH

    body_bytes = b""
    if body is not None:
        body_bytes = json.dumps(body).encode("utf-8")
        env["CONTENT_LENGTH"] = str(len(body_bytes))
        env["CONTENT_TYPE"] = "application/json"

    if token:
        env["HTTP_X_SESSION_TOKEN"] = token

    proc = subprocess.Popen(
        [EXE_PATH],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=BASE_DIR,
        env=env
    )
    stdout, stderr = proc.communicate(input=body_bytes)

    # Parse status and JSON
    text = stdout.decode("utf-8", errors="ignore")
    delimiter = "\r\n\r\n" if "\r\n\r\n" in text else ("\n\n" if "\n\n" in text else "\r\r\n\r\r\n")
    if delimiter in text:
        header_part, body_str = text.split(delimiter, 1)
    else:
        header_part = ""
        body_str = text

    status_code = 200
    for line in header_part.splitlines():
        if line.lower().startswith("status:"):
            try:
                status_code = int(line.split(":", 1)[1].strip().split()[0])
            except:
                pass

    try:
        data = json.loads(body_str.strip())
    except:
        data = {"raw": body_str}

    return status_code, data

class TestHospitalManagementSystem(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.path.exists(EXE_PATH):
            raise RuntimeError(f"C binary not found at {EXE_PATH}. Please compile first.")

    def test_01_user_registration(self):
        username = "testpatient_auto"
        email = "testpatient_auto@hospital.com"
        status, res = cgi_request("POST", "/api/auth/register", {
            "username": username,
            "email": email,
            "password": "password123",
            "full_name": "Test Patient Automated",
            "phone": "9811122233",
            "date_of_birth": "1994-04-12",
            "gender": "Male",
            "blood_group": "A+",
            "address": "Auto Test Street",
            "emergency_contact": "Guardian 9811122234",
            "role": "patient"
        })
        self.assertIn(status, [200, 409], f"Registration failed: {res}")
        if status == 200:
            self.assertTrue(res.get("success"))
            self.assertIn("user_id", res.get("data", {}))

    def test_02_invalid_password(self):
        status, res = cgi_request("POST", "/api/auth/login", {
            "username": "admin",
            "password": "wrong_password_xyz"
        })
        self.assertEqual(status, 401)
        self.assertFalse(res.get("success"))

    def test_03_login_and_logout(self):
        # Login admin
        status, res = cgi_request("POST", "/api/auth/login", {
            "username": "admin",
            "password": "admin123"
        })
        self.assertEqual(status, 200)
        self.assertTrue(res.get("success"))
        token = res["data"]["token"]
        self.assertTrue(len(token) > 10)

        # Check me with token
        status_me, res_me = cgi_request("GET", "/api/auth/me", token=token)
        self.assertEqual(status_me, 200)
        self.assertEqual(res_me["data"]["username"], "admin")
        self.assertEqual(res_me["data"]["role"], "admin")

        # Logout
        status_out, res_out = cgi_request("POST", "/api/auth/logout", token=token)
        self.assertEqual(status_out, 200)
        self.assertTrue(res_out.get("success"))

    def test_04_role_authorization(self):
        # Login patient
        status, res = cgi_request("POST", "/api/auth/login", {
            "username": "patient1",
            "password": "patient123"
        })
        self.assertEqual(status, 200)
        patient_token = res["data"]["token"]

        # Attempt to add doctor as patient (should be forbidden: 403)
        status_doc, res_doc = cgi_request("POST", "/api/doctors", {
            "full_name": "Dr. Unauthorized",
            "department_id": 1,
            "specialization": "Surgery"
        }, token=patient_token)
        self.assertEqual(status_doc, 403)
        self.assertFalse(res_doc.get("success"))

    def test_05_patient_creation_and_search(self):
        # Login admin
        _, res = cgi_request("POST", "/api/auth/login", {"username": "admin", "password": "admin123"})
        token = res["data"]["token"]

        # Create patient
        status, p_res = cgi_request("POST", "/api/patients", {
            "full_name": "Ananya Sen Gupta",
            "date_of_birth": "1992-07-20",
            "gender": "Female",
            "phone": "9823456789",
            "blood_group": "B+",
            "address": "Kolkata, India",
            "emergency_contact": "Sourav Gupta 9823456788"
        }, token=token)
        self.assertEqual(status, 200)
        self.assertTrue(p_res.get("success"))
        self.assertIn("patient_id", p_res["data"])

        # Search patient
        status_s, s_res = cgi_request("GET", "/api/patients", token=token, query="search=Ananya")
        self.assertEqual(status_s, 200)
        self.assertTrue(len(s_res.get("data", [])) >= 1)

    def test_06_doctor_creation(self):
        _, res = cgi_request("POST", "/api/auth/login", {"username": "admin", "password": "admin123"})
        token = res["data"]["token"]

        status, d_res = cgi_request("POST", "/api/doctors", {
            "full_name": "Dr. Siddhartha Mukherjee",
            "department_id": 1,
            "specialization": "Clinical Hematology & Oncology",
            "qualification": "MBBS, MD, DPhil",
            "consultation_fee": 1200.0,
            "availability": "10:00 - 15:00"
        }, token=token)
        self.assertEqual(status, 200)
        self.assertTrue(d_res.get("success"))
        self.assertIn("doctor_id", d_res["data"])

    def test_07_appointment_booking_and_double_booking_prevention(self):
        _, res = cgi_request("POST", "/api/auth/login", {"username": "admin", "password": "admin123"})
        token = res["data"]["token"]

        # Slot date and time
        slot_date = "2026-12-15"
        slot_time = "10:30"

        # Book first time -> Success
        status1, res1 = cgi_request("POST", "/api/appointments", {
            "patient_id": 1,
            "doctor_id": 2,
            "appointment_date": slot_date,
            "appointment_time": slot_time,
            "reason": "Test first booking"
        }, token=token)
        self.assertEqual(status1, 200)
        self.assertTrue(res1.get("success"))

        # Book second time with same doctor, date & time -> Must be rejected with 409 Conflict
        status2, res2 = cgi_request("POST", "/api/appointments", {
            "patient_id": 2,
            "doctor_id": 2,
            "appointment_date": slot_date,
            "appointment_time": slot_time,
            "reason": "Duplicate collision attempt"
        }, token=token)
        self.assertEqual(status2, 409, f"Double booking prevention failed! Output: {res2}")
        self.assertFalse(res2.get("success"))

    def test_08_appointment_cancellation(self):
        _, res = cgi_request("POST", "/api/auth/login", {"username": "admin", "password": "admin123"})
        token = res["data"]["token"]

        # Book an appointment to cancel
        status_b, res_b = cgi_request("POST", "/api/appointments", {
            "patient_id": 1,
            "doctor_id": 3,
            "appointment_date": "2026-12-28",
            "appointment_time": "15:00",
            "reason": "Booking to cancel"
        }, token=token)
        appt_id = res_b["data"]["appointment_id"]

        # Cancel appointment
        status_c, res_c = cgi_request("POST", "/api/appointments/cancel", {"id": appt_id}, token=token)
        self.assertEqual(status_c, 200)
        self.assertTrue(res_c.get("success"))

    def test_09_medical_record_and_prescription_creation(self):
        # Login doctor
        _, res = cgi_request("POST", "/api/auth/login", {"username": "dr.smith", "password": "doctor123"})
        doc_token = res["data"]["token"]
        doc_id = res["data"].get("doctor_id", 1)

        # Create EMR record with embedded prescription
        status, emr_res = cgi_request("POST", "/api/medical-records", {
            "patient_id": 1,
            "doctor_id": doc_id,
            "diagnosis": "Seasonal Allergic Rhinitis",
            "consultation_notes": "Mild nasal congestion, clear chest auscultation. Advised hydration.",
            "prescriptions": [
                {
                    "medicine_name": "Cetirizine 10mg",
                    "dosage": "1 Tab",
                    "frequency": "Once daily at bedtime",
                    "duration": "7 Days",
                    "instructions": "Take with water"
                }
            ]
        }, token=doc_token)
        self.assertEqual(status, 200)
        self.assertTrue(emr_res.get("success"))

    def test_10_pharmacy_stock_updates(self):
        _, res = cgi_request("POST", "/api/auth/login", {"username": "admin", "password": "admin123"})
        token = res["data"]["token"]

        # Add medicine
        status_add, m_add = cgi_request("POST", "/api/medicines", {
            "medicine_name": "Automated Test Drug 250mg",
            "category": "Antiviral",
            "quantity": 50,
            "unit_price": 85.0,
            "expiry_date": "2027-10-10",
            "minimum_stock": 15
        }, token=token)
        self.assertEqual(status_add, 200)
        med_id = m_add["data"]["medicine_id"]

        # Update stock quantity
        status_up, _ = cgi_request("PUT", "/api/medicines", {
            "id": med_id,
            "quantity": 10  # Less than min stock to trigger alert
        }, token=token)
        self.assertEqual(status_up, 200)

        # Verify low stock alert flag
        status_get, m_get = cgi_request("GET", "/api/medicines", token=token, query="low_stock=true")
        self.assertEqual(status_get, 200)
        names = [x["medicine_name"] for x in m_get["data"]]
        self.assertIn("Automated Test Drug 250mg", names)

    def test_11_invoice_and_simulated_payment(self):
        _, res = cgi_request("POST", "/api/auth/login", {"username": "admin", "password": "admin123"})
        token = res["data"]["token"]

        # Create invoice
        status_inv, inv_res = cgi_request("POST", "/api/invoices", {
            "patient_id": 1,
            "subtotal": 1000.0
        }, token=token)
        self.assertEqual(status_inv, 200)
        inv_id = inv_res["data"]["invoice_id"]
        total_amt = inv_res["data"]["total_amount"]
        self.assertEqual(total_amt, 1050.0) # Subtotal 1000 + 5% GST (50)

        # Record simulated payment
        status_pay, pay_res = cgi_request("POST", "/api/payments", {
            "invoice_id": inv_id,
            "amount": total_amt,
            "payment_method": "UPI"
        }, token=token)
        self.assertEqual(status_pay, 200)
        self.assertTrue(pay_res.get("success"))
        self.assertEqual(pay_res["data"]["payment_status"], "Paid")
        self.assertTrue(pay_res["data"]["reference_number"].startswith("PAY-UPI-"))

    def test_12_emergency_priority_management(self):
        _, res = cgi_request("POST", "/api/auth/login", {"username": "admin", "password": "admin123"})
        token = res["data"]["token"]

        # Admit emergency patient
        status_emg, emg_res = cgi_request("POST", "/api/emergency", {
            "patient_id": 2,
            "reported_symptoms": "Acute shortness of breath, SpO2 88%",
            "priority": "Critical"
        }, token=token)
        self.assertEqual(status_emg, 200)
        case_id = emg_res["data"]["emergency_case_id"]

        # Update status to In-Treatment
        status_up, _ = cgi_request("PUT", "/api/emergency", {
            "id": case_id,
            "status": "In-Treatment"
        }, token=token)
        self.assertEqual(status_up, 200)

    def test_13_dashboard_and_revenue_reports(self):
        _, res = cgi_request("POST", "/api/auth/login", {"username": "admin", "password": "admin123"})
        token = res["data"]["token"]

        status_dash, dash_res = cgi_request("GET", "/api/reports/dashboard", token=token)
        self.assertEqual(status_dash, 200)
        self.assertTrue(dash_res.get("success"))
        d = dash_res["data"]
        self.assertIn("total_patients", d)
        self.assertIn("total_doctors", d)
        self.assertIn("total_appointments", d)
        self.assertIn("total_revenue", d)
        self.assertIn("completion_rate", d)

        status_rev, rev_res = cgi_request("GET", "/api/reports/revenue", token=token)
        self.assertEqual(status_rev, 200)
        self.assertTrue(rev_res.get("success"))
        r = rev_res["data"]
        self.assertIn("total_revenue_collected", r)
        self.assertIn("by_payment_method", r)
        self.assertIn("by_department", r)

if __name__ == "__main__":
    unittest.main(verbosity=2)
