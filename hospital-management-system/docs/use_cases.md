# UML Use Case Diagram - Hospital Management System

```mermaid
flowchart LR
    subgraph Actors
        A((Administrator))
        D((Doctor))
        R((Receptionist))
        P((Patient))
    end

    subgraph SystemBoundary["Hospital Management System Boundary"]
        UC1(["Register / Login to Portal"])
        UC2(["Manage Hospital Doctors & Staff"])
        UC3(["View Analytics & Revenue Reports"])
        UC4(["Register Walk-In Patient"])
        UC5(["Book Outpatient Appointment"])
        UC6(["Cancel / Reschedule Appointment"])
        UC7(["Prevent Slot Double-Booking"])
        UC8(["Record Diagnosis & EMR Findings"])
        UC9(["Prescribe Medications"])
        UC10(["Manage Pharmacy Inventory & Low-Stock"])
        UC11(["Generate Invoice with 5% GST"])
        UC12(["Record Simulated Payment in INR"])
        UC13(["Admit Patient into Emergency Triage"])
        UC14(["Track Emergency Bed / Waiting Status"])
    end

    P --> UC1
    P --> UC5
    P --> UC6
    P --> UC12

    R --> UC1
    R --> UC4
    R --> UC5
    R --> UC6
    R --> UC11
    R --> UC12
    R --> UC13

    D --> UC1
    D --> UC8
    D --> UC9
    D --> UC14

    A --> UC1
    A --> UC2
    A --> UC3
    A --> UC10
    A --> UC11
    A --> UC13

    UC5 -.->|includes| UC7
    UC8 -.->|extends| UC9
    UC5 -.->|triggers| UC11
```

## Role Breakdown

| Role | Primary Responsibilities | Core Access |
|---|---|---|
| **Administrator** | Hospital governance, staff onboarding, financial analytics, system settings | Full Superuser Access to all tables and configurations |
| **Doctor** | Consultation queue, clinical diagnoses, electronic health records, prescriptions | Patient records, clinical EMR, prescription authoring, emergency queue |
| **Receptionist** | Front desk triage, walk-in patient intake, appointment scheduling, billing & cashier | Patients, appointment slots, invoices, simulated payment collection |
| **Patient** | Self-service registration, doctor directory search, appointment booking, invoices | Personal medical records, personal appointments, invoice receipts |

