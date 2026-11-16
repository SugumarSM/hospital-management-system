# Database Entity-Relationship Diagram (ERD)

The SQLite relational schema for the Hospital Management System (`hospital.db`) contains 13 relational tables linked with foreign keys, uniqueness constraints, and indexing.

```mermaid
erDiagram
    users ||--o| patients : "linked to"
    users ||--o| doctors : "linked to"
    users ||--o{ sessions : "has active"
    users ||--o{ audit_logs : "performed by"
    departments ||--o{ doctors : "categorizes"
    patients ||--o{ appointments : "schedules"
    doctors ||--o{ appointments : "conducts"
    patients ||--o{ medical_records : "has history"
    doctors ||--o{ medical_records : "authors"
    appointments ||--o| medical_records : "documents"
    medical_records ||--o{ prescriptions : "prescribes"
    patients ||--o{ prescriptions : "receives"
    doctors ||--o{ prescriptions : "signs"
    patients ||--o{ invoices : "billed to"
    appointments ||--o| invoices : "charges for"
    invoices ||--o{ payments : "settled with"
    patients ||--o{ emergency_cases : "admitted in"

    users {
        int id PK
        string username UK
        string email UK
        string password_hash
        string role
        string status
        timestamp created_at
    }

    sessions {
        int id PK
        string session_token UK
        int user_id FK
        bigint expires_at
        timestamp created_at
    }

    departments {
        int id PK
        string department_name UK
        string description
    }

    doctors {
        int id PK
        int user_id FK
        string doctor_code UK
        string full_name
        int department_id FK
        string specialization
        string qualification
        real consultation_fee
        string availability
        string status
    }

    patients {
        int id PK
        string patient_code UK
        int user_id FK
        string full_name
        string date_of_birth
        string gender
        string phone
        string address
        string emergency_contact
        string blood_group
        timestamp created_at
    }

    appointments {
        int id PK
        int patient_id FK
        int doctor_id FK
        string appointment_date
        string appointment_time
        string reason
        string status
        timestamp created_at
    }

    medical_records {
        int id PK
        int patient_id FK
        int doctor_id FK
        int appointment_id FK
        string diagnosis
        string consultation_notes
        timestamp created_at
    }

    prescriptions {
        int id PK
        int patient_id FK
        int doctor_id FK
        int medical_record_id FK
        string medicine_name
        string dosage
        string frequency
        string duration
        string instructions
        timestamp created_at
    }

    medicines {
        int id PK
        string medicine_name UK
        string category
        int quantity
        real unit_price
        string expiry_date
        int minimum_stock
    }

    invoices {
        int id PK
        int patient_id FK
        int appointment_id FK
        real subtotal
        real tax
        real total_amount
        string payment_status
        timestamp created_at
    }

    payments {
        int id PK
        int invoice_id FK
        real amount
        string payment_method
        timestamp payment_date
        string reference_number UK
    }

    emergency_cases {
        int id PK
        int patient_id FK
        string reported_symptoms
        string priority
        string status
        timestamp created_at
    }

    audit_logs {
        int id PK
        int user_id FK
        string action
        string entity_type
        int entity_id
        string details
        timestamp created_at
    }
```

