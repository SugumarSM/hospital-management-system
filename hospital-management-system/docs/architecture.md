# System Architecture - Medicare Hospital Management System

## 1. High-Level Architectural Diagram

The system employs a multi-tiered architecture with a native C11 CGI backend application layer, SQLite transactional database engine, and a responsive Bootstrap 5 / Vanilla JavaScript presentation layer.

```mermaid
flowchart TD
    subgraph Client["Presentation Layer (Client Browser)"]
        UI["Web Browser (HTML5 / CSS3 / Vanilla JS)"]
        ChartJS["Chart.js Analytics Engine"]
        UI -->|Asynchronous JSON API Calls| Router["HTTP Client / Fetch API"]
    end

    subgraph Server["Web Server Infrastructure"]
        WebServer["Apache HTTP Server / Micro Web Server (:8080)"]
        CGIHandler["CGI Gateway (RFC 3875)"]
        WebServer -->|Executes with env vars & stdin| CGIHandler
    end

    subgraph Backend["C Application Layer (C11 Standard)"]
        Main["main.c (Router & Controller)"]
        Config["config.c (Configuration & Environment)"]
        Session["session.c (Token & Session Store)"]
        Auth["auth.c (Argon2 / SHA-256 Hashing & RBAC)"]
        Validation["validation.c (Input Sanitization)"]
        Audit["audit.c (HIPAA Audit Trail)"]

        subgraph Modules["Hospital Business Logic Modules"]
            PatientMod["patient.c"]
            DoctorMod["doctor.c"]
            ApptMod["appointment.c"]
            EMRMod["medical_records.c"]
            RxMod["prescription.c"]
            PharmMod["pharmacy.c"]
            BillMod["billing.c"]
            EmgMod["emergency.c"]
            RepMod["reports.c"]
        end

        JSONLib["cJSON Engine"]
        Response["response.c (CGI Headers & JSON Formatting)"]
    end

    subgraph DataTier["Data Access Layer"]
        DBConn["database.c (SQLite C API)"]
        Transactions["ACID Transactions & Prepared Statements"]
        SQLiteDB[("SQLite 3 Database (hospital.db)")]
    end

    Router -->|HTTP Request / JSON| WebServer
    CGIHandler --> Main
    Main --> Auth
    Main --> Session
    Main --> Validation
    Main --> Modules
    Modules --> DBConn
    DBConn --> Transactions
    Transactions --> SQLiteDB
    Modules --> Response
    Response -->|JSON Response (stdout)| CGIHandler
    CGIHandler -->|HTTP 200/400/401/403/409| Router
```

## 2. Layer Description

### Layer 1: Presentation Tier
- Built strictly with HTML5, responsive CSS3, and modern Vanilla JavaScript without bloated heavy frontend dependencies.
- Handles user interface rendering, form client validation, authentication token management via `localStorage` and `SameSite` cookies, and dynamic Chart.js data visualization.

### Layer 2: Web Server & CGI Execution Tier
- Standard Apache HTTP Server configured with `ScriptAlias /api/` pointing to the compiled C executable (`hospital_api.cgi` / `hospital_api.exe`).
- Implements CGI/1.1 specification passing `REQUEST_METHOD`, `PATH_INFO`, `QUERY_STRING`, `CONTENT_LENGTH`, and `HTTP_COOKIE`.
- For standalone portability, also includes a lightweight zero-configuration HTTP server runner (`run_server.py`).

### Layer 3: C Application Business Logic Tier
- Implemented strictly in C (C11 standard) using GCC.
- Modular decomposition: separate header (`.h`) and implementation (`.c`) files for each hospital domain.
- Secure session state management, constant-time password verification, and backend permission verification (enforced at API level, not just hidden UI buttons).

### Layer 4: Data Storage & Persistence Tier
- Embedded SQLite engine utilizing prepared statements (`sqlite3_prepare_v2`, `sqlite3_bind_*`, `sqlite3_step`) to completely eliminate SQL injection vulnerabilities.
- Foreign key enforcement enabled via `PRAGMA foreign_keys = ON;`.
- Concurrency and data integrity protected by explicit database transactions (`BEGIN TRANSACTION`, `COMMIT`, `ROLLBACK`).

