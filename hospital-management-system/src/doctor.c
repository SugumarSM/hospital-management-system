#include "doctor.h"
#include "response.h"
#include "json_utils.h"
#include "validation.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_doctors_get(sqlite3 *db, const AuthUser *user, const char *query_string) {
    (void)user;
    int dept_id = 0;
    int active_only = 0;

    if (query_string) {
        const char *dept_ptr = strstr(query_string, "department_id=");
        if (dept_ptr) dept_id = atoi(dept_ptr + 14);
        if (strstr(query_string, "status=active")) active_only = 1;
    }

    char sql[512] = "SELECT d.id, d.doctor_code, d.full_name, d.department_id, dep.department_name, "
                    "d.specialization, d.qualification, d.consultation_fee, d.availability, d.status "
                    "FROM doctors d JOIN departments dep ON d.department_id = dep.id WHERE 1=1";

    if (dept_id > 0) {
        strcat(sql, " AND d.department_id = ?");
    }
    if (active_only) {
        strcat(sql, " AND d.status = 'active'");
    }
    strcat(sql, " ORDER BY d.full_name ASC");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database query preparation failed");
        return;
    }

    if (dept_id > 0) {
        sqlite3_bind_int(stmt, 1, dept_id);
    }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(row, "doctor_code", (const char*)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(row, "full_name", (const char*)sqlite3_column_text(stmt, 2));
        cJSON_AddNumberToObject(row, "department_id", sqlite3_column_int(stmt, 3));
        cJSON_AddStringToObject(row, "department_name", (const char*)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(row, "specialization", (const char*)sqlite3_column_text(stmt, 5));
        cJSON_AddStringToObject(row, "qualification", (const char*)sqlite3_column_text(stmt, 6));
        cJSON_AddNumberToObject(row, "consultation_fee", sqlite3_column_double(stmt, 7));
        cJSON_AddStringToObject(row, "availability", (const char*)sqlite3_column_text(stmt, 8));
        cJSON_AddStringToObject(row, "status", (const char*)sqlite3_column_text(stmt, 9));
        cJSON_AddItemToArray(arr, row);
    }
    sqlite3_finalize(stmt);

    response_send_success("Doctors retrieved successfully", arr, NULL);
}

void handle_doctors_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user || strcmp(user->role, "admin") != 0) {
        response_send_error(403, "Only administrators can add doctors");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    const char *full_name = json_get_string(body, "full_name", NULL);
    int department_id = json_get_int(body, "department_id", 0);
    const char *specialization = json_get_string(body, "specialization", NULL);
    const char *qualification = json_get_string(body, "qualification", NULL);
    double consultation_fee = json_get_double(body, "consultation_fee", 500.0);
    const char *availability = json_get_string(body, "availability", "09:00 - 17:00");

    if (!validate_not_empty(full_name)) {
        response_send_error(400, "Doctor's full name is required");
        return;
    }
    if (department_id <= 0) {
        response_send_error(400, "Valid department ID is required");
        return;
    }
    if (!validate_not_empty(specialization)) {
        response_send_error(400, "Specialization is required");
        return;
    }

    // Generate unique doctor code
    sqlite3_stmt *cstmt = NULL;
    int next_id = 1001;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(MAX(id), 0) + 1 FROM doctors", -1, &cstmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(cstmt) == SQLITE_ROW) {
            next_id = 1000 + sqlite3_column_int(cstmt, 0);
        }
        sqlite3_finalize(cstmt);
    }
    char doctor_code[32];
    snprintf(doctor_code, sizeof(doctor_code), "DOC-%04d", next_id);

    const char *sql = "INSERT INTO doctors (doctor_code, full_name, department_id, specialization, qualification, consultation_fee, availability, status) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, 'active')";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_text(stmt, 1, doctor_code, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, full_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, department_id);
    sqlite3_bind_text(stmt, 4, specialization, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, qualification ? qualification : "MBBS", -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 6, consultation_fee);
    sqlite3_bind_text(stmt, 7, availability, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to insert doctor record");
        return;
    }
    sqlite3_finalize(stmt);

    int new_id = (int)sqlite3_last_insert_rowid(db);
    audit_log_action(db, user->user_id, "CREATE_DOCTOR", "DOCTOR", new_id, full_name);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "doctor_id", new_id);
    cJSON_AddStringToObject(data, "doctor_code", doctor_code);

    response_send_success("Doctor added successfully", data, NULL);
}

void handle_doctors_put(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user || strcmp(user->role, "admin") != 0) {
        response_send_error(403, "Only administrators can update doctor details");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int id = json_get_int(body, "id", 0);
    if (id <= 0) {
        response_send_error(400, "Valid doctor ID is required");
        return;
    }

    const char *full_name = json_get_string(body, "full_name", NULL);
    int department_id = json_get_int(body, "department_id", 0);
    const char *specialization = json_get_string(body, "specialization", NULL);
    const char *qualification = json_get_string(body, "qualification", NULL);
    const char *availability = json_get_string(body, "availability", NULL);
    const char *status = json_get_string(body, "status", NULL);
    cJSON *fee_item = cJSON_GetObjectItemCaseSensitive(body, "consultation_fee");

    const char *sql = "UPDATE doctors SET "
                      "full_name = COALESCE(?, full_name), "
                      "department_id = CASE WHEN ? > 0 THEN ? ELSE department_id END, "
                      "specialization = COALESCE(?, specialization), "
                      "qualification = COALESCE(?, qualification), "
                      "availability = COALESCE(?, availability), "
                      "status = COALESCE(?, status), "
                      "consultation_fee = CASE WHEN ? >= 0 THEN ? ELSE consultation_fee END "
                      "WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    if (full_name) sqlite3_bind_text(stmt, 1, full_name, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 1);
    sqlite3_bind_int(stmt, 2, department_id);
    sqlite3_bind_int(stmt, 3, department_id);
    if (specialization) sqlite3_bind_text(stmt, 4, specialization, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 4);
    if (qualification) sqlite3_bind_text(stmt, 5, qualification, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 5);
    if (availability) sqlite3_bind_text(stmt, 6, availability, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 6);
    if (status) sqlite3_bind_text(stmt, 7, status, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 7);
    if (fee_item && cJSON_IsNumber(fee_item)) {
        sqlite3_bind_double(stmt, 8, fee_item->valuedouble);
        sqlite3_bind_double(stmt, 9, fee_item->valuedouble);
    } else {
        sqlite3_bind_double(stmt, 8, -1.0);
        sqlite3_bind_double(stmt, 9, -1.0);
    }
    sqlite3_bind_int(stmt, 10, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to update doctor record");
        return;
    }
    sqlite3_finalize(stmt);

    audit_log_action(db, user->user_id, "UPDATE_DOCTOR", "DOCTOR", id, status ? status : "Updated details");
    response_send_success("Doctor updated successfully", NULL, NULL);
}

void handle_departments_get(sqlite3 *db) {
    const char *sql = "SELECT id, department_name, description FROM departments ORDER BY department_name ASC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(row, "department_name", (const char*)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(row, "description", sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "");
        cJSON_AddItemToArray(arr, row);
    }
    sqlite3_finalize(stmt);

    response_send_success("Departments retrieved successfully", arr, NULL);
}

void handle_departments_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user || strcmp(user->role, "admin") != 0) {
        response_send_error(403, "Only administrators can add departments");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    const char *name = json_get_string(body, "department_name", NULL);
    const char *desc = json_get_string(body, "description", "");

    if (!validate_not_empty(name)) {
        response_send_error(400, "Department name is required");
        return;
    }

    const char *sql = "INSERT INTO departments (department_name, description) VALUES (?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, desc, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(409, "Department already exists or could not be added");
        return;
    }
    sqlite3_finalize(stmt);

    int new_id = (int)sqlite3_last_insert_rowid(db);
    audit_log_action(db, user->user_id, "CREATE_DEPARTMENT", "DEPARTMENT", new_id, name);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "department_id", new_id);
    cJSON_AddStringToObject(data, "department_name", name);

    response_send_success("Department added successfully", data, NULL);
}

