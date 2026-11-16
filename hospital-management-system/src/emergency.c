#include "emergency.h"
#include "response.h"
#include "json_utils.h"
#include "validation.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_emergency_get(sqlite3 *db, const AuthUser *user, const char *query_string) {
    (void)user;
    char status_filter[32] = "";
    if (query_string) {
        const char *p = strstr(query_string, "status=");
        if (p) {
            const char *val = p + 7;
            const char *end = strchr(val, '&');
            size_t len = end ? (size_t)(end - val) : strlen(val);
            if (len >= sizeof(status_filter)) len = sizeof(status_filter) - 1;
            strncpy(status_filter, val, len);
            status_filter[len] = '\0';
        }
    }

    char sql[512] = "SELECT e.id, e.patient_id, p.full_name as patient_name, p.patient_code, p.phone, p.blood_group, "
                    "e.reported_symptoms, e.priority, e.status, e.created_at "
                    "FROM emergency_cases e "
                    "JOIN patients p ON e.patient_id = p.id WHERE 1=1";

    if (strlen(status_filter) > 0) {
        strcat(sql, " AND e.status = ?");
    }

    // Sort by priority (Critical > High > Medium > Low) and then time
    strcat(sql, " ORDER BY CASE e.priority "
                "WHEN 'Critical' THEN 1 "
                "WHEN 'High' THEN 2 "
                "WHEN 'Medium' THEN 3 "
                "WHEN 'Low' THEN 4 ELSE 5 END ASC, e.created_at ASC LIMIT 100");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    if (strlen(status_filter) > 0) {
        sqlite3_bind_text(stmt, 1, status_filter, -1, SQLITE_STATIC);
    }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddNumberToObject(row, "patient_id", sqlite3_column_int(stmt, 1));
        cJSON_AddStringToObject(row, "patient_name", (const char*)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(row, "patient_code", (const char*)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(row, "phone", (const char*)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(row, "blood_group", sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "");
        cJSON_AddStringToObject(row, "reported_symptoms", (const char*)sqlite3_column_text(stmt, 6));
        cJSON_AddStringToObject(row, "priority", (const char*)sqlite3_column_text(stmt, 7));
        cJSON_AddStringToObject(row, "status", (const char*)sqlite3_column_text(stmt, 8));
        cJSON_AddStringToObject(row, "created_at", (const char*)sqlite3_column_text(stmt, 9));
        cJSON_AddItemToArray(arr, row);
    }
    sqlite3_finalize(stmt);

    response_send_success("Emergency queue retrieved", arr, NULL);
}

void handle_emergency_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "receptionist") != 0 && strcmp(user->role, "doctor") != 0) {
        response_send_error(403, "Insufficient permissions to register emergency admissions");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int patient_id = json_get_int(body, "patient_id", 0);
    const char *symptoms = json_get_string(body, "reported_symptoms", NULL);
    const char *priority = json_get_string(body, "priority", "High");

    if (patient_id <= 0) {
        response_send_error(400, "Valid patient ID is required");
        return;
    }
    if (!validate_not_empty(symptoms)) {
        response_send_error(400, "Reported symptoms are required for emergency admission");
        return;
    }

    const char *sql = "INSERT INTO emergency_cases (patient_id, reported_symptoms, priority, status) VALUES (?, ?, ?, 'Waiting')";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_int(stmt, 1, patient_id);
    sqlite3_bind_text(stmt, 2, symptoms, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, priority, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to register emergency case");
        return;
    }
    sqlite3_finalize(stmt);

    int new_case_id = (int)sqlite3_last_insert_rowid(db);
    audit_log_action(db, user->user_id, "EMERGENCY_ADMISSION", "EMERGENCY", new_case_id, priority);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "emergency_case_id", new_case_id);
    cJSON_AddStringToObject(data, "priority", priority);
    cJSON_AddStringToObject(data, "status", "Waiting");

    response_send_success("Emergency patient registered in triage queue", data, NULL);
}

void handle_emergency_put(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "doctor") != 0 && strcmp(user->role, "receptionist") != 0) {
        response_send_error(403, "Insufficient permissions to update emergency status");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int id = json_get_int(body, "id", 0);
    const char *new_status = json_get_string(body, "status", NULL);
    const char *new_priority = json_get_string(body, "priority", NULL);

    if (id <= 0) {
        response_send_error(400, "Valid emergency case ID is required");
        return;
    }

    const char *sql = "UPDATE emergency_cases SET "
                      "status = COALESCE(?, status), "
                      "priority = COALESCE(?, priority) "
                      "WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    if (new_status) sqlite3_bind_text(stmt, 1, new_status, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 1);
    if (new_priority) sqlite3_bind_text(stmt, 2, new_priority, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_int(stmt, 3, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to update emergency case");
        return;
    }
    sqlite3_finalize(stmt);

    audit_log_action(db, user->user_id, "UPDATE_EMERGENCY", "EMERGENCY", id, new_status ? new_status : new_priority);
    response_send_success("Emergency case updated successfully", NULL, NULL);
}

