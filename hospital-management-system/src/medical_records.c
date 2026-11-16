#include "medical_records.h"
#include "auth.h"
#include "response.h"
#include "json_utils.h"
#include "validation.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_medical_records_get(sqlite3 *db, const AuthUser *user, const char *query_string) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }

    int patient_id = 0;
    int record_id = 0;

    if (query_string) {
        const char *p = strstr(query_string, "patient_id=");
        if (p) patient_id = atoi(p + 11);
        p = strstr(query_string, "id=");
        if (p) record_id = atoi(p + 3);
    }

    // Role restrictions: Patients can only view their own records
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
            response_send_success("Medical records", cJSON_CreateArray(), NULL);
            return;
        }
    }

    char sql[1024] = "SELECT mr.id, mr.patient_id, p.full_name as patient_name, p.patient_code, "
                     "mr.doctor_id, d.full_name as doctor_name, dep.department_name, "
                     "mr.appointment_id, mr.diagnosis, mr.consultation_notes, mr.created_at "
                     "FROM medical_records mr "
                     "JOIN patients p ON mr.patient_id = p.id "
                     "JOIN doctors d ON mr.doctor_id = d.id "
                     "JOIN departments dep ON d.department_id = dep.id "
                     "WHERE 1=1";

    if (record_id > 0) strcat(sql, " AND mr.id = ?");
    if (patient_id > 0) strcat(sql, " AND mr.patient_id = ?");
    strcat(sql, " ORDER BY mr.created_at DESC LIMIT 100");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    int b_idx = 1;
    if (record_id > 0) sqlite3_bind_int(stmt, b_idx++, record_id);
    if (patient_id > 0) sqlite3_bind_int(stmt, b_idx++, patient_id);

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        int mid = sqlite3_column_int(stmt, 0);
        cJSON_AddNumberToObject(row, "id", mid);
        cJSON_AddNumberToObject(row, "patient_id", sqlite3_column_int(stmt, 1));
        cJSON_AddStringToObject(row, "patient_name", (const char*)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(row, "patient_code", (const char*)sqlite3_column_text(stmt, 3));
        cJSON_AddNumberToObject(row, "doctor_id", sqlite3_column_int(stmt, 4));
        cJSON_AddStringToObject(row, "doctor_name", (const char*)sqlite3_column_text(stmt, 5));
        cJSON_AddStringToObject(row, "department_name", (const char*)sqlite3_column_text(stmt, 6));
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
            cJSON_AddNumberToObject(row, "appointment_id", sqlite3_column_int(stmt, 7));
        } else {
            cJSON_AddNullToObject(row, "appointment_id");
        }
        cJSON_AddStringToObject(row, "diagnosis", (const char*)sqlite3_column_text(stmt, 8));
        cJSON_AddStringToObject(row, "consultation_notes", sqlite3_column_text(stmt, 9) ? (const char*)sqlite3_column_text(stmt, 9) : "");
        cJSON_AddStringToObject(row, "created_at", (const char*)sqlite3_column_text(stmt, 10));

        // Fetch associated prescriptions for this medical record
        cJSON *rx_arr = cJSON_CreateArray();
        sqlite3_stmt *rx_stmt = NULL;
        const char *rx_sql = "SELECT id, medicine_name, dosage, frequency, duration, instructions FROM prescriptions WHERE medical_record_id = ?";
        if (sqlite3_prepare_v2(db, rx_sql, -1, &rx_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(rx_stmt, 1, mid);
            while (sqlite3_step(rx_stmt) == SQLITE_ROW) {
                cJSON *rx_item = cJSON_CreateObject();
                cJSON_AddNumberToObject(rx_item, "id", sqlite3_column_int(rx_stmt, 0));
                cJSON_AddStringToObject(rx_item, "medicine_name", (const char*)sqlite3_column_text(rx_stmt, 1));
                cJSON_AddStringToObject(rx_item, "dosage", (const char*)sqlite3_column_text(rx_stmt, 2));
                cJSON_AddStringToObject(rx_item, "frequency", (const char*)sqlite3_column_text(rx_stmt, 3));
                cJSON_AddStringToObject(rx_item, "duration", (const char*)sqlite3_column_text(rx_stmt, 4));
                cJSON_AddStringToObject(rx_item, "instructions", sqlite3_column_text(rx_stmt, 5) ? (const char*)sqlite3_column_text(rx_stmt, 5) : "");
                cJSON_AddItemToArray(rx_arr, rx_item);
            }
            sqlite3_finalize(rx_stmt);
        }
        cJSON_AddItemToObject(row, "prescriptions", rx_arr);

        cJSON_AddItemToArray(arr, row);
    }
    sqlite3_finalize(stmt);

    response_send_success("Medical records retrieved successfully", arr, NULL);
}

void handle_medical_records_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    // Only doctors or admins can create medical records
    if (!auth_is_medical_staff(user)) {
        response_send_error(403, "Access denied: Only licensed medical doctors can create clinical records");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int patient_id = json_get_int(body, "patient_id", 0);
    int doctor_id = json_get_int(body, "doctor_id", 0);
    int appointment_id = json_get_int(body, "appointment_id", 0);
    const char *diagnosis = json_get_string(body, "diagnosis", NULL);
    const char *notes = json_get_string(body, "consultation_notes", "");

    // If user is doctor, auto-resolve doctor_id
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
    if (!validate_not_empty(diagnosis)) {
        response_send_error(400, "Clinical diagnosis is required");
        return;
    }

    const char *sql = "INSERT INTO medical_records (patient_id, doctor_id, appointment_id, diagnosis, consultation_notes) "
                      "VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_int(stmt, 1, patient_id);
    sqlite3_bind_int(stmt, 2, doctor_id);
    if (appointment_id > 0) sqlite3_bind_int(stmt, 3, appointment_id); else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_text(stmt, 4, diagnosis, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, notes, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to create medical record");
        return;
    }
    sqlite3_finalize(stmt);

    int new_record_id = (int)sqlite3_last_insert_rowid(db);

    // If linked to an appointment, mark appointment as Completed
    if (appointment_id > 0) {
        sqlite3_stmt *astmt = NULL;
        if (sqlite3_prepare_v2(db, "UPDATE appointments SET status = 'Completed' WHERE id = ?", -1, &astmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(astmt, 1, appointment_id);
            sqlite3_step(astmt);
            sqlite3_finalize(astmt);
        }
    }

    // Process optional array of prescriptions included in the payload
    cJSON *prescriptions = cJSON_GetObjectItemCaseSensitive(body, "prescriptions");
    if (prescriptions && cJSON_IsArray(prescriptions)) {
        int count = cJSON_GetArraySize(prescriptions);
        const char *rx_sql = "INSERT INTO prescriptions (patient_id, doctor_id, medical_record_id, medicine_name, dosage, frequency, duration, instructions) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        sqlite3_stmt *rx_stmt = NULL;
        if (sqlite3_prepare_v2(db, rx_sql, -1, &rx_stmt, NULL) == SQLITE_OK) {
            for (int i = 0; i < count; i++) {
                cJSON *item = cJSON_GetArrayItem(prescriptions, i);
                const char *med_name = json_get_string(item, "medicine_name", NULL);
                const char *dosage = json_get_string(item, "dosage", "1 Tablet");
                const char *freq = json_get_string(item, "frequency", "Once daily");
                const char *dur = json_get_string(item, "duration", "7 Days");
                const char *inst = json_get_string(item, "instructions", "After meals");
                if (med_name && strlen(med_name) > 0) {
                    sqlite3_bind_int(rx_stmt, 1, patient_id);
                    sqlite3_bind_int(rx_stmt, 2, doctor_id);
                    sqlite3_bind_int(rx_stmt, 3, new_record_id);
                    sqlite3_bind_text(rx_stmt, 4, med_name, -1, SQLITE_STATIC);
                    sqlite3_bind_text(rx_stmt, 5, dosage, -1, SQLITE_STATIC);
                    sqlite3_bind_text(rx_stmt, 6, freq, -1, SQLITE_STATIC);
                    sqlite3_bind_text(rx_stmt, 7, dur, -1, SQLITE_STATIC);
                    sqlite3_bind_text(rx_stmt, 8, inst, -1, SQLITE_STATIC);
                    sqlite3_step(rx_stmt);
                    sqlite3_reset(rx_stmt);
                }
            }
            sqlite3_finalize(rx_stmt);
        }
    }

    audit_log_action(db, user->user_id, "CREATE_EMR", "MEDICAL_RECORD", new_record_id, diagnosis);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "medical_record_id", new_record_id);
    response_send_success("Medical record created successfully", data, NULL);
}

