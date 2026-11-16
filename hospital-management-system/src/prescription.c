#include "prescription.h"
#include "auth.h"
#include "response.h"
#include "json_utils.h"
#include "validation.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_prescriptions_get(sqlite3 *db, const AuthUser *user, const char *query_string) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }

    int patient_id = 0;
    int record_id = 0;

    if (query_string) {
        const char *p = strstr(query_string, "patient_id=");
        if (p) patient_id = atoi(p + 11);
        p = strstr(query_string, "medical_record_id=");
        if (p) record_id = atoi(p + 18);
    }

    // Role restrictions: Patients can only view their own prescriptions
    if (strcmp(user->role, "patient") == 0) {
        sqlite3_stmt *own = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id FROM patients WHERE user_id = ?", -1, &own, NULL) == SQLITE_OK) {
            sqlite3_bind_int(own, 1, user->user_id);
            if (sqlite3_step(own) == SQLITE_ROW) {
                patient_id = sqlite3_column_int(own, 0);
            }
            sqlite3_finalize(own);
        }
        if (patient_id <= 0) {
            response_send_success("Prescriptions retrieved", cJSON_CreateArray(), NULL);
            return;
        }
    }

    char sql[1024] = "SELECT rx.id, rx.patient_id, p.full_name as patient_name, p.patient_code, "
                     "rx.doctor_id, d.full_name as doctor_name, dep.department_name, "
                     "rx.medical_record_id, rx.medicine_name, rx.dosage, rx.frequency, rx.duration, rx.instructions, rx.created_at "
                     "FROM prescriptions rx "
                     "JOIN patients p ON rx.patient_id = p.id "
                     "JOIN doctors d ON rx.doctor_id = d.id "
                     "JOIN departments dep ON d.department_id = dep.id "
                     "WHERE 1=1";

    if (patient_id > 0) strcat(sql, " AND rx.patient_id = ?");
    if (record_id > 0) strcat(sql, " AND rx.medical_record_id = ?");
    strcat(sql, " ORDER BY rx.created_at DESC LIMIT 150");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    int b_idx = 1;
    if (patient_id > 0) sqlite3_bind_int(stmt, b_idx++, patient_id);
    if (record_id > 0) sqlite3_bind_int(stmt, b_idx++, record_id);

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddNumberToObject(row, "patient_id", sqlite3_column_int(stmt, 1));
        cJSON_AddStringToObject(row, "patient_name", (const char*)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(row, "patient_code", (const char*)sqlite3_column_text(stmt, 3));
        cJSON_AddNumberToObject(row, "doctor_id", sqlite3_column_int(stmt, 4));
        cJSON_AddStringToObject(row, "doctor_name", (const char*)sqlite3_column_text(stmt, 5));
        cJSON_AddStringToObject(row, "department_name", (const char*)sqlite3_column_text(stmt, 6));
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
            cJSON_AddNumberToObject(row, "medical_record_id", sqlite3_column_int(stmt, 7));
        } else {
            cJSON_AddNullToObject(row, "medical_record_id");
        }
        cJSON_AddStringToObject(row, "medicine_name", (const char*)sqlite3_column_text(stmt, 8));
        cJSON_AddStringToObject(row, "dosage", (const char*)sqlite3_column_text(stmt, 9));
        cJSON_AddStringToObject(row, "frequency", (const char*)sqlite3_column_text(stmt, 10));
        cJSON_AddStringToObject(row, "duration", (const char*)sqlite3_column_text(stmt, 11));
        cJSON_AddStringToObject(row, "instructions", sqlite3_column_text(stmt, 12) ? (const char*)sqlite3_column_text(stmt, 12) : "");
        cJSON_AddStringToObject(row, "created_at", (const char*)sqlite3_column_text(stmt, 13));
        cJSON_AddItemToArray(arr, row);
    }
    sqlite3_finalize(stmt);

    response_send_success("Prescriptions retrieved successfully", arr, NULL);
}

void handle_prescriptions_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (!auth_is_medical_staff(user)) {
        response_send_error(403, "Access denied: Only medical doctors can issue prescriptions");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int patient_id = json_get_int(body, "patient_id", 0);
    int doctor_id = json_get_int(body, "doctor_id", 0);
    int medical_record_id = json_get_int(body, "medical_record_id", 0);
    const char *medicine_name = json_get_string(body, "medicine_name", NULL);
    const char *dosage = json_get_string(body, "dosage", "1 Tablet");
    const char *frequency = json_get_string(body, "frequency", "Once daily");
    const char *duration = json_get_string(body, "duration", "7 Days");
    const char *instructions = json_get_string(body, "instructions", "After food");

    // If doctor role, auto-resolve doctor_id
    if (strcmp(user->role, "doctor") == 0) {
        sqlite3_stmt *own = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id FROM doctors WHERE user_id = ?", -1, &own, NULL) == SQLITE_OK) {
            sqlite3_bind_int(own, 1, user->user_id);
            if (sqlite3_step(own) == SQLITE_ROW) {
                doctor_id = sqlite3_column_int(own, 0);
            }
            sqlite3_finalize(own);
        }
    }

    if (patient_id <= 0) {
        response_send_error(400, "Valid patient ID is required");
        return;
    }
    if (doctor_id <= 0) {
        response_send_error(400, "Valid doctor ID is required");
        return;
    }
    if (!validate_not_empty(medicine_name)) {
        response_send_error(400, "Medicine name is required");
        return;
    }

    const char *sql = "INSERT INTO prescriptions (patient_id, doctor_id, medical_record_id, medicine_name, dosage, frequency, duration, instructions) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_int(stmt, 1, patient_id);
    sqlite3_bind_int(stmt, 2, doctor_id);
    if (medical_record_id > 0) sqlite3_bind_int(stmt, 3, medical_record_id); else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, medicine_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, dosage, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, frequency, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, duration, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, instructions, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to create prescription");
        return;
    }
    sqlite3_finalize(stmt);

    int new_rx_id = (int)sqlite3_last_insert_rowid(db);
    audit_log_action(db, user->user_id, "CREATE_PRESCRIPTION", "PRESCRIPTION", new_rx_id, medicine_name);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "prescription_id", new_rx_id);
    response_send_success("Prescription issued successfully", data, NULL);
}

