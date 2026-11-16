#include "patient.h"
#include "response.h"
#include "json_utils.h"
#include "validation.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_patients_get(sqlite3 *db, const AuthUser *user, const char *query_string) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }

    int patient_id = 0;
    char search[128] = "";

    if (query_string) {
        const char *id_ptr = strstr(query_string, "id=");
        if (id_ptr) {
            patient_id = atoi(id_ptr + 3);
        }
        const char *search_ptr = strstr(query_string, "search=");
        if (search_ptr) {
            const char *val = search_ptr + 7;
            const char *end = strchr(val, '&');
            size_t len = end ? (size_t)(end - val) : strlen(val);
            if (len >= sizeof(search)) len = sizeof(search) - 1;
            strncpy(search, val, len);
            search[len] = '\0';
        }
    }

    // Role-based restrictions: Patient role can only see their own record
    if (strcmp(user->role, "patient") == 0) {
        sqlite3_stmt *own = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id FROM patients WHERE user_id = ?", -1, &own, NULL) == SQLITE_OK) {
            sqlite3_bind_int(own, 1, user->user_id);
            if (sqlite3_step(own) == SQLITE_ROW) {
                patient_id = sqlite3_column_int(own, 0);
            } else {
                sqlite3_finalize(own);
                response_send_error(404, "No patient profile associated with this account");
                return;
            }
            sqlite3_finalize(own);
        }
    }

    if (patient_id > 0) {
        // Fetch single patient
        const char *sql = "SELECT id, patient_code, full_name, date_of_birth, gender, phone, address, emergency_contact, blood_group, created_at "
                          "FROM patients WHERE id = ?";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            response_send_error(500, "Database error");
            return;
        }
        sqlite3_bind_int(stmt, 1, patient_id);

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            response_send_error(404, "Patient not found");
            return;
        }

        cJSON *p_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(p_obj, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(p_obj, "patient_code", (const char*)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(p_obj, "full_name", (const char*)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(p_obj, "date_of_birth", (const char*)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(p_obj, "gender", (const char*)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(p_obj, "phone", (const char*)sqlite3_column_text(stmt, 5));
        cJSON_AddStringToObject(p_obj, "address", (const char*)sqlite3_column_text(stmt, 6));
        cJSON_AddStringToObject(p_obj, "emergency_contact", (const char*)sqlite3_column_text(stmt, 7));
        cJSON_AddStringToObject(p_obj, "blood_group", sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "");
        cJSON_AddStringToObject(p_obj, "created_at", (const char*)sqlite3_column_text(stmt, 9));
        sqlite3_finalize(stmt);

        // Fetch recent appointments for this patient
        cJSON *appts_arr = cJSON_CreateArray();
        const char *a_sql = "SELECT a.id, a.appointment_date, a.appointment_time, a.status, a.reason, d.full_name "
                            "FROM appointments a JOIN doctors d ON a.doctor_id = d.id "
                            "WHERE a.patient_id = ? ORDER BY a.appointment_date DESC, a.appointment_time DESC LIMIT 10";
        sqlite3_stmt *astmt = NULL;
        if (sqlite3_prepare_v2(db, a_sql, -1, &astmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(astmt, 1, patient_id);
            while (sqlite3_step(astmt) == SQLITE_ROW) {
                cJSON *a_item = cJSON_CreateObject();
                cJSON_AddNumberToObject(a_item, "id", sqlite3_column_int(astmt, 0));
                cJSON_AddStringToObject(a_item, "appointment_date", (const char*)sqlite3_column_text(astmt, 1));
                cJSON_AddStringToObject(a_item, "appointment_time", (const char*)sqlite3_column_text(astmt, 2));
                cJSON_AddStringToObject(a_item, "status", (const char*)sqlite3_column_text(astmt, 3));
                cJSON_AddStringToObject(a_item, "reason", sqlite3_column_text(astmt, 4) ? (const char*)sqlite3_column_text(astmt, 4) : "");
                cJSON_AddStringToObject(a_item, "doctor_name", (const char*)sqlite3_column_text(astmt, 5));
                cJSON_AddItemToArray(appts_arr, a_item);
            }
            sqlite3_finalize(astmt);
        }
        cJSON_AddItemToObject(p_obj, "appointments", appts_arr);

        response_send_success("Patient retrieved successfully", p_obj, NULL);
        return;
    }

    // List patients
    char sql[512] = "SELECT id, patient_code, full_name, date_of_birth, gender, phone, address, emergency_contact, blood_group, created_at FROM patients";
    if (strlen(search) > 0) {
        strcat(sql, " WHERE full_name LIKE ? OR phone LIKE ? OR patient_code LIKE ?");
    }
    strcat(sql, " ORDER BY id DESC LIMIT 100");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    if (strlen(search) > 0) {
        char param[132];
        snprintf(param, sizeof(param), "%%%s%%", search);
        sqlite3_bind_text(stmt, 1, param, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, param, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, param, -1, SQLITE_TRANSIENT);
    }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(row, "patient_code", (const char*)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(row, "full_name", (const char*)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(row, "date_of_birth", (const char*)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(row, "gender", (const char*)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(row, "phone", (const char*)sqlite3_column_text(stmt, 5));
        cJSON_AddStringToObject(row, "address", (const char*)sqlite3_column_text(stmt, 6));
        cJSON_AddStringToObject(row, "emergency_contact", (const char*)sqlite3_column_text(stmt, 7));
        cJSON_AddStringToObject(row, "blood_group", sqlite3_column_text(stmt, 8) ? (const char*)sqlite3_column_text(stmt, 8) : "");
        cJSON_AddStringToObject(row, "created_at", (const char*)sqlite3_column_text(stmt, 9));
        cJSON_AddItemToArray(arr, row);
    }
    sqlite3_finalize(stmt);

    response_send_success("Patients listed successfully", arr, NULL);
}

void handle_patients_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    // Only admin, receptionist or doctor can register a walk-in patient
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "receptionist") != 0 && strcmp(user->role, "doctor") != 0) {
        response_send_error(403, "Insufficient permissions to register patients");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    const char *full_name = json_get_string(body, "full_name", NULL);
    const char *dob = json_get_string(body, "date_of_birth", NULL);
    const char *gender = json_get_string(body, "gender", "Male");
    const char *phone = json_get_string(body, "phone", NULL);
    const char *address = json_get_string(body, "address", "Not provided");
    const char *emergency_contact = json_get_string(body, "emergency_contact", "Not provided");
    const char *blood_group = json_get_string(body, "blood_group", "O+");

    if (!validate_not_empty(full_name)) {
        response_send_error(400, "Full name is required");
        return;
    }
    if (!validate_date(dob)) {
        response_send_error(400, "Valid date of birth (YYYY-MM-DD) is required");
        return;
    }
    if (!validate_phone(phone)) {
        response_send_error(400, "Valid phone number is required");
        return;
    }

    // Generate unique patient code
    sqlite3_stmt *cstmt = NULL;
    int next_id = 1001;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(MAX(id), 0) + 1 FROM patients", -1, &cstmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(cstmt) == SQLITE_ROW) {
            next_id = 1000 + sqlite3_column_int(cstmt, 0);
        }
        sqlite3_finalize(cstmt);
    }
    char patient_code[32];
    snprintf(patient_code, sizeof(patient_code), "PAT-%04d", next_id);

    const char *sql = "INSERT INTO patients (patient_code, full_name, date_of_birth, gender, phone, address, emergency_contact, blood_group) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_text(stmt, 1, patient_code, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, dob, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, gender, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, address, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, emergency_contact, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, blood_group, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to insert patient record");
        return;
    }
    sqlite3_finalize(stmt);

    int new_id = (int)sqlite3_last_insert_rowid(db);
    audit_log_action(db, user->user_id, "CREATE_PATIENT", "PATIENT", new_id, full_name);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "patient_id", new_id);
    cJSON_AddStringToObject(data, "patient_code", patient_code);
    cJSON_AddStringToObject(data, "full_name", full_name);

    response_send_success("Patient registered successfully", data, NULL);
}

void handle_patients_put(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int id = json_get_int(body, "id", 0);
    if (id <= 0) {
        response_send_error(400, "Valid patient ID is required");
        return;
    }

    // Role check: If patient, must be updating own record
    if (strcmp(user->role, "patient") == 0) {
        sqlite3_stmt *chk = NULL;
        int match = 0;
        if (sqlite3_prepare_v2(db, "SELECT id FROM patients WHERE id = ? AND user_id = ?", -1, &chk, NULL) == SQLITE_OK) {
            sqlite3_bind_int(chk, 1, id);
            sqlite3_bind_int(chk, 2, user->user_id);
            if (sqlite3_step(chk) == SQLITE_ROW) {
                match = 1;
            }
            sqlite3_finalize(chk);
        }
        if (!match) {
            response_send_error(403, "You can only update your own patient profile");
            return;
        }
    }

    const char *full_name = json_get_string(body, "full_name", NULL);
    const char *dob = json_get_string(body, "date_of_birth", NULL);
    const char *gender = json_get_string(body, "gender", NULL);
    const char *phone = json_get_string(body, "phone", NULL);
    const char *address = json_get_string(body, "address", NULL);
    const char *emergency_contact = json_get_string(body, "emergency_contact", NULL);
    const char *blood_group = json_get_string(body, "blood_group", NULL);

    const char *sql = "UPDATE patients SET "
                      "full_name = COALESCE(?, full_name), "
                      "date_of_birth = COALESCE(?, date_of_birth), "
                      "gender = COALESCE(?, gender), "
                      "phone = COALESCE(?, phone), "
                      "address = COALESCE(?, address), "
                      "emergency_contact = COALESCE(?, emergency_contact), "
                      "blood_group = COALESCE(?, blood_group) "
                      "WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    if (full_name) sqlite3_bind_text(stmt, 1, full_name, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 1);
    if (dob) sqlite3_bind_text(stmt, 2, dob, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 2);
    if (gender) sqlite3_bind_text(stmt, 3, gender, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 3);
    if (phone) sqlite3_bind_text(stmt, 4, phone, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 4);
    if (address) sqlite3_bind_text(stmt, 5, address, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 5);
    if (emergency_contact) sqlite3_bind_text(stmt, 6, emergency_contact, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 6);
    if (blood_group) sqlite3_bind_text(stmt, 7, blood_group, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 7);
    sqlite3_bind_int(stmt, 8, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to update patient record");
        return;
    }
    sqlite3_finalize(stmt);

    audit_log_action(db, user->user_id, "UPDATE_PATIENT", "PATIENT", id, full_name);
    response_send_success("Patient updated successfully", NULL, NULL);
}

void handle_patients_delete(sqlite3 *db, const AuthUser *user, const char *query_string) {
    if (!user || strcmp(user->role, "admin") != 0) {
        response_send_error(403, "Only administrators can delete patient records");
        return;
    }

    int id = 0;
    if (query_string) {
        const char *id_ptr = strstr(query_string, "id=");
        if (id_ptr) id = atoi(id_ptr + 3);
    }
    if (id <= 0) {
        response_send_error(400, "Valid patient ID is required");
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM patients WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    audit_log_action(db, user->user_id, "DELETE_PATIENT", "PATIENT", id, NULL);
    response_send_success("Patient deleted successfully", NULL, NULL);
}

