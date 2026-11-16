# Testing Documentation - Hospital Management System

This document provides testing specifications, test execution instructions, and verified test cases for the C backend and full-stack Hospital Management System.

---

## 1. Test Execution Commands

To execute the automated regression test suite:

```bash
cd hospital-management-system
python tests/test_runner.py
```

Or using Makefile:
```bash
make test
```

---

## 2. Test Suite Matrix

| # | Test Case Description | Target Endpoint | Input Condition | Expected Status | Actual Status | Verification Method |
|---|---|---|---|---|---|---|
| **TC-01** | User Registration | `POST /api/auth/register` | Valid patient payload | 200 OK | **PASS** | Validates patient code & user ID creation |
| **TC-02** | Invalid Password Rejection | `POST /api/auth/login` | Wrong password for user | 401 Unauthorized | **PASS** | Constant-time hash check fails; security alert logged |
| **TC-03** | Login, Session & Logout | `POST /api/auth/login` & `logout` | Valid admin credentials | 200 OK | **PASS** | Generates secure 48-char token; `/auth/me` returns role; token invalidated on logout |
| **TC-04** | Role-Based Authorization | `POST /api/doctors` | Patient token creating doctor | 403 Forbidden | **PASS** | Backend rejects non-admin users with 403 error |
| **TC-05** | Patient Creation & Search | `POST /api/patients` & `GET ?search=` | New patient details | 200 OK | **PASS** | Inserts patient; verifies search filter matches |
| **TC-06** | Doctor Creation | `POST /api/doctors` | Admin creates specialist | 200 OK | **PASS** | Generates unique `DOC-xxxx` code and assigns department |
| **TC-07** | Slot Double-Booking Prevention | `POST /api/appointments` | 2 bookings for same doc/date/time | 409 Conflict | **PASS** | Second booking rejected with double booking error |
| **TC-08** | Appointment Cancellation | `POST /api/appointments/cancel` | Existing appointment ID | 200 OK | **PASS** | Status updated to 'Cancelled' in SQLite database |
| **TC-09** | Medical Record & Rx Creation | `POST /api/medical-records` | Doctor diagnoses consultation | 200 OK | **PASS** | Clinical notes stored; associated prescriptions linked |
| **TC-10** | Pharmacy Stock & Low-Stock Alert | `PUT /api/medicines` | Stock reduced below min threshold | 200 OK | **PASS** | Filter `low_stock=true` detects flag |
| **TC-11** | Invoicing & Simulated Payment | `POST /api/invoices` & `/payments` | Invoice subtotal + 5% GST | 200 OK | **PASS** | Status transitions to 'Paid'; unique payment reference generated |
| **TC-12** | Emergency Admission & Priority | `POST /api/emergency` & `PUT` | Critical triage patient | 200 OK | **PASS** | Case queued by priority; status transitions from Waiting to In-Treatment |
| **TC-13** | Dashboard Reports & BI | `GET /api/reports/dashboard` | Admin requesting analytics | 200 OK | **PASS** | Validates KPI totals, revenue in ₹, completion rate, Chart.js payload |

---

## 3. Sample Test Output Log

```text
test_01_user_registration (__main__.TestHospitalManagementSystem.test_01_user_registration) ... ok
test_02_invalid_password (__main__.TestHospitalManagementSystem.test_02_invalid_password) ... ok
test_03_login_and_logout (__main__.TestHospitalManagementSystem.test_03_login_and_logout) ... ok
test_04_role_authorization (__main__.TestHospitalManagementSystem.test_04_role_authorization) ... ok
test_05_patient_creation_and_search (__main__.TestHospitalManagementSystem.test_05_patient_creation_and_search) ... ok
test_06_doctor_creation (__main__.TestHospitalManagementSystem.test_06_doctor_creation) ... ok
test_07_appointment_booking_and_double_booking_prevention (__main__.TestHospitalManagementSystem.test_07_appointment_booking_and_double_booking_prevention) ... ok
test_08_appointment_cancellation (__main__.TestHospitalManagementSystem.test_08_appointment_cancellation) ... ok
test_09_medical_record_and_prescription_creation (__main__.TestHospitalManagementSystem.test_09_medical_record_and_prescription_creation) ... ok
test_10_pharmacy_stock_updates (__main__.TestHospitalManagementSystem.test_10_pharmacy_stock_updates) ... ok
test_11_invoice_and_simulated_payment (__main__.TestHospitalManagementSystem.test_11_invoice_and_simulated_payment) ... ok
test_12_emergency_priority_management (__main__.TestHospitalManagementSystem.test_12_emergency_priority_management) ... ok
test_13_dashboard_and_revenue_reports (__main__.TestHospitalManagementSystem.test_13_dashboard_and_revenue_reports) ... ok

----------------------------------------------------------------------
Ran 13 tests in 1.006s

OK
```

