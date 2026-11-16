# Advanced Hospital Management System (C Backend)

A full-stack, enterprise-grade Hospital Management System engineered with a **C11 backend**, **embedded SQLite3 database**, and a modern **HTML5/Bootstrap 5/Vanilla JavaScript/Chart.js frontend**. Built as a final-year college capstone project.

---

## 🌟 Key Features

- **Pure C11 Backend:** All hospital business logic, validation, authentication, and database operations execute natively in C. No Node.js, Python Flask/Django, PHP, or Java backend frameworks are used.
- **Role-Based Access Control (RBAC):** Backend-enforced authorization across 4 distinct roles:
  - 👑 **Admin:** Hospital governance, doctor management, department oversight, financial analytics, audit logs.
  - 🩺 **Doctor:** Assigned consultations, patient medical records, clinical diagnoses, and prescriptions.
  - 🛎️ **Receptionist:** Walk-in patient intake, appointment scheduling, patient check-in, cashier invoicing.
  - 👤 **Patient:** Self-service registration, appointment booking, medical history, prescriptions, and invoice payments.
- **Smart Appointment Scheduling:** Concurrency-safe slot allocation preventing double booking (`UNIQUE(doctor_id, date, time)` returning HTTP 409 Conflict).
- **Electronic Medical Records (EMR):** Clinical diagnosis notes and observations strictly restricted to licensed medical practitioners.
- **Pharmacy Inventory Control:** Stock level tracking, unit pricing, batch expiry alerts, and automated low-stock warnings.
- **INR (₹) Billing & Invoicing:** 5% GST calculations, simulated payment recording (Cash, Card, UPI, NetBanking), and printable tax receipts.
- **Emergency Triage Queue:** Prioritized emergency queue (Critical, High, Medium, Low) and bed status tracking.
- **Analytics & BI Dashboard:** Interactive Chart.js charts displaying revenue trends, department distribution, and appointment completion rates.

---

## 🛠️ Technology Stack

| Layer | Technologies |
|---|---|
| **Frontend** | HTML5, CSS3, Vanilla JavaScript (ES6+), Chart.js |
| **Backend** | C Programming Language (C11 standard), GCC |
| **Database** | SQLite 3 (using SQLite C Amalgamation API) |
| **JSON Parser** | cJSON (DaveGamble) |
| **Web Server** | Apache HTTP Server (CGI RFC 3875) / Python CGI Local Runner |
| **Security** | Salted SHA-256 (Windows CryptoAPI CNG), secure token sessions, prepared SQL statements |

---

## 📂 Project Directory Structure

```text
hospital-management-system/
├── frontend/
│   ├── index.html                   # Public Hospital Landing Page
│   ├── login.html                   # Login with 1-Click Demo Credentials
│   ├── register.html                # Patient Registration Form
│   ├── admin-dashboard.html         # Admin Dashboard with KPI Cards & Charts
│   ├── doctor-dashboard.html        # Doctor Consultation Queue & Diagnosis
│   ├── receptionist-dashboard.html  # Reception Desk, Walk-ins & Check-in
│   ├── patient-dashboard.html       # Patient Health Records & Invoices
│   ├── patients.html                # Patient Directory & Search
│   ├── doctors.html                 # Doctors Directory & Add Doctor Modal
│   ├── departments.html             # Specialty Departments
│   ├── appointments.html            # Outpatient Scheduling & Conflict Check
│   ├── medical-records.html         # Electronic Medical Records (EMR)
│   ├── prescriptions.html           # Prescriptions & Printable Slips
│   ├── pharmacy.html                # Pharmacy Stock & Low-Stock Alerts
│   ├── billing.html                 # Invoices, 5% GST & Simulated Payment in ₹
│   ├── emergency.html               # Emergency Triage Queue
│   ├── profile.html                 # User Profile Management
│   ├── css/
│   │   └── style.css                # Modern Medical Theme Stylesheet
│   └── js/
│       ├── api.js                   # Unified C API Client & Toasts
│       └── auth.js                  # RBAC Guard & Dynamic Navigation
├── src/
│   ├── main.c                       # Master CGI Router & Controller
│   ├── config.c                     # Configuration Management
│   ├── database.c                   # SQLite Connection & Transactions
│   ├── auth.c                       # Password Hashing & Authentication
│   ├── session.c                    # Secure Session State Management
│   ├── validation.c                 # Input Sanitization & Validation
│   ├── patient.c                    # Patient Business Logic
│   ├── doctor.c                     # Doctor & Department Operations
│   ├── appointment.c                # Concurrency-Safe Appointment Logic
│   ├── medical_records.c            # EMR Documentation Logic
│   ├── prescription.c               # Prescription Dispensing Logic
│   ├── pharmacy.c                   # Inventory Stock Control
│   ├── billing.c                    # Invoicing & Payment Processing
│   ├── emergency.c                  # Emergency Triage Logic
│   ├── reports.c                    # Analytical Aggregations
│   ├── json_utils.c                 # Safe JSON Extraction Helpers
│   ├── response.c                   # HTTP Headers & JSON Serialization
│   ├── audit.c                      # Audit Logging Trail
│   ├── cJSON.c                      # cJSON Parsing Engine
│   └── sqlite3.c                    # Embedded SQLite Engine
├── include/                         # Header files for all modules (.h)
├── database/
│   ├── schema.sql                   # Relational Schema Definition
│   ├── seed.sql                     # Demonstration Data
│   └── hospital.db                  # SQLite Database
├── build/
│   ├── hospital_api.exe             # Compiled C Executable (Windows)
│   └── hospital_api.cgi             # Compiled CGI Binary
├── apache/
│   └── hospital.conf                # Apache VirtualHost & CGI Config
├── tests/
│   └── test_runner.py               # Automated Regression Test Suite (13 tests)
├── docs/
│   ├── architecture.md              # System Architecture & Mermaid Diagram
│   ├── er_diagram.md                # Database ER Diagram (13 tables)
│   ├── data_flow.md                 # Data Flow Diagrams (Level 0 & 1)
│   ├── use_cases.md                 # UML Use Case Diagram
│   ├── project_report.md            # Final Capstone Project Report
│   └── testing.md                   # Verification & Test Cases Matrix
├── Makefile                         # Compilation & Build Automation
└── run_server.py                    # Turnkey Local Web Server Runner
```

---

## 🔑 Demonstration Accounts

The database comes pre-seeded with accounts for all roles. You can use the **1-Click Quick Fill buttons** on `login.html`:

| Role | Username | Password | Full Name / Description |
|---|---|---|---|
| **Admin** | `admin` | `admin123` | Hospital Executive Administrator |
| **Doctor** | `dr.smith` | `doctor123` | Dr. John Smith (Cardiology Specialist) |
| **Doctor** | `dr.johnson` | `doctor123` | Dr. Emily Johnson (Neurology Specialist) |
| **Receptionist** | `receptionist` | `recep123` | Front Desk Receptionist & Cashier |
| **Patient** | `patient1` | `patient123` | Rahul Verma (Patient Code: PAT-1001) |
| **Patient** | `patient2` | `patient123` | Priya Sharma (Patient Code: PAT-1002) |

---

## 🚀 Quick Start & Execution

### Prerequisites
- GCC Compiler (C11 standard support)
- Python 3.8+ (for local server runner and database seeding scripts)

### Step 1: Initialize Database (Already done)
If you ever want to reset the database to clean seed data:
```bash
make init-db
```

### Step 2: Compile the C Backend
```bash
make
```
*Compiles all 19 C modules and links `build/hospital_api.exe` and `build/hospital_api.cgi`.*

### Step 3: Run the Application Locally
Run the turnkey local server runner:
```bash
python run_server.py
```
Or with Make:
```bash
make run
```
Open your browser to:
👉 **[http://localhost:8080](http://localhost:8080)**

---

## 🌐 Running on Apache HTTP Server (Production / Formal Submission)

1. Ensure the Apache `mod_cgi` module is enabled in `httpd.conf`:
   ```apache
   LoadModule cgi_module modules/mod_cgi.so
   ```
2. Include the provided configuration file (`apache/hospital.conf`) in your Apache `httpd.conf` or `conf.d/`:
   ```apache
   Include "path/to/hospital-management-system/apache/hospital.conf"
   ```
3. Restart Apache:
   - **Windows:** `httpd -k restart` or via Apache Service Monitor
   - **Linux:** `sudo systemctl restart apache2` or `sudo systemctl restart httpd`
4. Access the hospital portal via `http://localhost:8080`.

---

## 🧪 Running Automated Tests

Run the complete 13-test regression suite:
```bash
python tests/test_runner.py
```
Or with Make:
```bash
make test
```
**Test Results:**
```text
test_01_user_registration ... ok
test_02_invalid_password ... ok
test_03_login_and_logout ... ok
test_04_role_authorization ... ok
test_05_patient_creation_and_search ... ok
test_06_doctor_creation ... ok
test_07_appointment_booking_and_double_booking_prevention ... ok
test_08_appointment_cancellation ... ok
test_09_medical_record_and_prescription_creation ... ok
test_10_pharmacy_stock_updates ... ok
test_11_invoice_and_simulated_payment ... ok
test_12_emergency_priority_management ... ok
test_13_dashboard_and_revenue_reports ... ok
----------------------------------------------------------------------
Ran 13 tests in 1.006s -> OK
```

---

## 🛡️ Security & Privacy Notice
*This software is an academic demonstration project created for educational and final-year capstone review. It is not intended for use with actual patient protected health information (PHI) without formal regulatory compliance audits (such as HIPAA, GDPR, or DISHA).*

