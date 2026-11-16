#include "appointment.h"
#include "response.h"
#include "json_utils.h"
#include "validation.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_appointments_get(sqlite3 *db, const AuthUser *user, const char *query_string) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }

    int filter_patient_id = 0;
    int filter_doctor_id = 0;
    char filter_date[32] = "";
    char filter_status[32] = "";

    if (query_string) {
        const char *p = strstr(query_string, "patient_id=");
        if (p) filter_patient_id = atoi(p + 11);
        p = strstr(query_string, "doctor_id=");
        if (p) filter_doctor_id = atoi(p + 10);
        p = strstr(query_string, "date=");
        if (p) {
            const char *val = p + 5;
            const char *end = strchr(val, '&');
            size_t len = end ? (size_t)(end - val) : strlen(val);
            if (len >= sizeof(filter_date)) len = sizeof(filter_date) - 1;
            strncpy(filter_date, val, len);
            filter_date[len] = '\0';
        }
        p = strstr(query_string, "status=");
        if (p) {
            const char *val = p + 7;
            const char *end = strchr(val, '&');
            size_t len = end ? (size_t)(end - val) : strlen(val);
            if (len >= sizeof(filter_status)) len = sizeof(filter_status) - 1;
            strncpy(filter_status, val, len);
            filter_status[len] = '\0';
        }
    }

    // Role-based restrictions
    if (strcmp(user->role, "patient") == 0) {
        sqlite3_stmt *own = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id FROM patients WHERE user_id = ?", -1, &own, NULL) == SQLITE_OK) {
            sqlite3_bind_int(own, 1, user->user_id);
            if (sqlite3_step(own) == SQLITE_ROW) {
                filter_patient_id = sqlite3_column_int(own, 0);
            }
            sqlite3_finalize(own);
        }
        if (filter_patient_id <= 0) {
            response_send_success("Appointments listed", cJSON_CreateArray(), NULL);
            return;
        }
    } else if (strcmp(user->role, "doctor") == 0) {
        sqlite3_stmt *own = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id FROM doctors WHERE user_id = ?", -1, &own, NULL) == SQLITE_OK) {
            sqlite3_bind_int(own, 1, user->user_id);
            if (sqlite3_step(own) == SQLITE_ROW) {
                filter_doctor_id = sqlite3_column_int(own, 0);
            }
            sqlite3_finalize(own);
        }
    }

    char sql[1024] = "SELECT a.id, a.patient_id, p.patient_code, p.full_name as patient_name, p.phone as patient_phone, "
                     "a.doctor_id, d.doctor_code, d.full_name as doctor_name, dep.department_name, "
                     "a.appointment_date, a.appointment_time, a.reason, a.status, a.created_at, d.consultation_fee "
                     "FROM appointments a "
                     "JOIN patients p ON a.patient_id = p.id "
                     "JOIN doctors d ON a.doctor_id = d.id "
                     "JOIN departments dep ON d.department_id = dep.id "
                     "WHERE 1=1";

    if (filter_patient_id > 0) strcat(sql, " AND a.patient_id = ?");
    if (filter_doctor_id > 0) strcat(sql, " AND a.doctor_id = ?");
    if (strlen(filter_date) > 0) strcat(sql, " AND a.appointment_date = ?");
    if (strlen(filter_status) > 0) strcat(sql, " AND a.status = ?");

    strcat(sql, " ORDER BY a.appointment_date DESC, a.appointment_time DESC LIMIT 150");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database query preparation failed");
        return;
    }

    int b_idx = 1;
    if (filter_patient_id > 0) sqlite3_bind_int(stmt, b_idx++, filter_patient_id);
    if (filter_doctor_id > 0) sqlite3_bind_int(stmt, b_idx++, filter_doctor_id);
    if (strlen(filter_date) > 0) sqlite3_bind_text(stmt, b_idx++, filter_date, -1, SQLITE_STATIC);
    if (strlen(filter_status) > 0) sqlite3_bind_text(stmt, b_idx++, filter_status, -1, SQLITE_STATIC);

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddNumberToObject(row, "patient_id", sqlite3_column_int(stmt, 1));
        cJSON_AddStringToObject(row, "patient_code", (const char*)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(row, "patient_name", (const char*)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(row, "patient_phone", (const char*)sqlite3_column_text(stmt, 4));
        cJSON_AddNumberToObject(row, "doctor_id", sqlite3_column_int(stmt, 5));
        cJSON_AddStringToObject(row, "doctor_code", (const char*)sqlite3_column_text(stmt, 6));
        cJSON_AddStringToObject(row, "doctor_name", (const char*)sqlite3_column_text(stmt, 7));
        cJSON_AddStringToObject(row, "department_name", (const char*)sqlite3_column_text(stmt, 8));
        cJSON_AddStringToObject(row, "appointment_date", (const char*)sqlite3_column_text(stmt, 9));
        cJSON_AddStringToObject(row, "appointment_time", (const char*)sqlite3_column_text(stmt, 10));
        cJSON_AddStringToObject(row, "reason", sqlite3_column_text(stmt, 11) ? (const char*)sqlite3_column_text(stmt, 11) : "");
        cJSON_AddStringToObject(row, "status", (const char*)sqlite3_column_text(stmt, 12));
        cJSON_AddStringToObject(row, "created_at", (const char*)sqlite3_column_text(stmt, 13));
        cJSON_AddNumberToObject(row, "consultation_fee", sqlite3_column_double(stmt, 14));
        cJSON_AddItemToArray(arr, row);
    }
    sqlite3_finalize(stmt);

    response_send_success("Appointments retrieved successfully", arr, NULL);
}

void handle_appointments_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int patient_id = json_get_int(body, "patient_id", 0);
    int doctor_id = json_get_int(body, "doctor_id", 0);
    const char *date = json_get_string(body, "appointment_date", NULL);
    const char *time_slot = json_get_string(body, "appointment_time", NULL);
    const char *reason = json_get_string(body, "reason", "General Consultation");

    // If patient role, force patient_id to be their own
    if (strcmp(user->role, "patient") == 0) {
        sqlite3_stmt *own = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id FROM patients WHERE user_id = ?", -1, &own, NULL) == SQLITE_OK) {
            sqlite3_bind_int(own, 1, user->user_id);
            if (sqlite3_step(own) == SQLITE_ROW) {
                patient_id = sqlite3_column_int(own, 0);
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
    if (!validate_date(date)) {
        response_send_error(400, "Valid appointment date (YYYY-MM-DD) is required");
        return;
    }
    if (!validate_not_empty(time_slot)) {
        response_send_error(400, "Appointment time slot is required");
        return;
    }

    // Double booking check: ensure no active appointment exists for this doctor at this date and time
    const char *chk_sql = "SELECT id FROM appointments WHERE doctor_id = ? AND appointment_date = ? AND appointment_time = ? AND status != 'Cancelled'";
    sqlite3_stmt *chk = NULL;
    if (sqlite3_prepare_v2(db, chk_sql, -1, &chk, NULL) == SQLITE_OK) {
        sqlite3_bind_int(chk, 1, doctor_id);
        sqlite3_bind_text(chk, 2, date, -1, SQLITE_STATIC);
        sqlite3_bind_text(chk, 3, time_slot, -1, SQLITE_STATIC);
        if (sqlite3_step(chk) == SQLITE_ROW) {
            sqlite3_finalize(chk);
            response_send_error(409, "Double booking prevented: This doctor is already booked for the selected date and time slot.");
            return;
        }
        sqlite3_finalize(chk);
    }

    const char *sql = "INSERT INTO appointments (patient_id, doctor_id, appointment_date, appointment_time, reason, status) "
                      "VALUES (?, ?, ?, ?, ?, 'Confirmed')";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_int(stmt, 1, patient_id);
    sqlite3_bind_int(stmt, 2, doctor_id);
    sqlite3_bind_text(stmt, 3, date, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, time_slot, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, reason, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to schedule appointment");
        return;
    }
    sqlite3_finalize(stmt);

    int appt_id = (int)sqlite3_last_insert_rowid(db);

    // Fetch doctor fee to create initial invoice
    double fee = 500.0;
    sqlite3_stmt *dstmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT consultation_fee FROM doctors WHERE id = ?", -1, &dstmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(dstmt, 1, doctor_id);
        if (sqlite3_step(dstmt) == SQLITE_ROW) {
            fee = sqlite3_column_double(dstmt, 0);
        }
        sqlite3_finalize(dstmt);
    }

    double tax = fee * 0.05; // 5% GST
    double total = fee + tax;
    sqlite3_stmt *inv = NULL;
    if (sqlite3_prepare_v2(db, "INSERT INTO invoices (patient_id, appointment_id, subtotal, tax, total_amount, payment_status) VALUES (?, ?, ?, ?, ?, 'Unpaid')", -1, &inv, NULL) == SQLITE_OK) {
        sqlite3_bind_int(inv, 1, patient_id);
        sqlite3_bind_int(inv, 2, appt_id);
        sqlite3_bind_double(inv, 3, fee);
        sqlite3_bind_double(inv, 4, tax);
        sqlite3_bind_double(inv, 5, total);
        sqlite3_step(inv);
        sqlite3_finalize(inv);
    }

    audit_log_action(db, user->user_id, "BOOK_APPOINTMENT", "APPOINTMENT", appt_id, date);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "appointment_id", appt_id);
    cJSON_AddStringToObject(data, "status", "Confirmed");

    response_send_success("Appointment booked successfully", data, NULL);
}

void handle_appointments_put(sqlite3 *db, const AuthUser *user, cJSON *body) {
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
        response_send_error(400, "Valid appointment ID is required");
        return;
    }

    const char *new_status = json_get_string(body, "status", NULL);
    const char *new_date = json_get_string(body, "appointment_date", NULL);
    const char *new_time = json_get_string(body, "appointment_time", NULL);
    const char *reason = json_get_string(body, "reason", NULL);

    // If rescheduling, check conflict
    if (new_date && new_time) {
        sqlite3_stmt *cstmt = NULL;
        int doc_id = 0;
        if (sqlite3_prepare_v2(db, "SELECT doctor_id FROM appointments WHERE id = ?", -1, &cstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(cstmt, 1, id);
            if (sqlite3_step(cstmt) == SQLITE_ROW) {
                doc_id = sqlite3_column_int(cstmt, 0);
            }
            sqlite3_finalize(cstmt);
        }

        const char *chk_sql = "SELECT id FROM appointments WHERE doctor_id = ? AND appointment_date = ? AND appointment_time = ? AND id != ? AND status != 'Cancelled'";
        sqlite3_stmt *chk = NULL;
        if (sqlite3_prepare_v2(db, chk_sql, -1, &chk, NULL) == SQLITE_OK) {
            sqlite3_bind_int(chk, 1, doc_id);
            sqlite3_bind_text(chk, 2, new_date, -1, SQLITE_STATIC);
            sqlite3_bind_text(chk, 3, new_time, -1, SQLITE_STATIC);
            sqlite3_bind_int(chk, 4, id);
            if (sqlite3_step(chk) == SQLITE_ROW) {
                sqlite3_finalize(chk);
                response_send_error(409, "Double booking prevented: New slot is already taken.");
                return;
            }
            sqlite3_finalize(chk);
        }
    }

    const char *sql = "UPDATE appointments SET "
                      "status = COALESCE(?, status), "
                      "appointment_date = COALESCE(?, appointment_date), "
                      "appointment_time = COALESCE(?, appointment_time), "
                      "reason = COALESCE(?, reason) "
                      "WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    if (new_status) sqlite3_bind_text(stmt, 1, new_status, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 1);
    if (new_date) sqlite3_bind_text(stmt, 2, new_date, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 2);
    if (new_time) sqlite3_bind_text(stmt, 3, new_time, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 3);
    if (reason) sqlite3_bind_text(stmt, 4, reason, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 4);
    sqlite3_bind_int(stmt, 5, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to update appointment");
        return;
    }
    sqlite3_finalize(stmt);

    audit_log_action(db, user->user_id, "UPDATE_APPOINTMENT", "APPOINTMENT", id, new_status ? new_status : "Rescheduled");
    response_send_success("Appointment updated successfully", NULL, NULL);
}

void handle_appointments_cancel(sqlite3 *db, const AuthUser *user, cJSON *body) {
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
        response_send_error(400, "Valid appointment ID is required");
        return;
    }

    // Role check: patient can cancel only their own appointment
    if (strcmp(user->role, "patient") == 0) {
        sqlite3_stmt *chk = NULL;
        int match = 0;
        const char *chk_sql = "SELECT a.id FROM appointments a JOIN patients p ON a.patient_id = p.id WHERE a.id = ? AND p.user_id = ?";
        if (sqlite3_prepare_v2(db, chk_sql, -1, &chk, NULL) == SQLITE_OK) {
            sqlite3_bind_int(chk, 1, id);
            sqlite3_bind_int(chk, 2, user->user_id);
            if (sqlite3_step(chk) == SQLITE_ROW) {
                match = 1;
            }
            sqlite3_finalize(chk);
        }
        if (!match) {
            response_send_error(403, "You can only cancel your own appointments");
            return;
        }
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE appointments SET status = 'Cancelled' WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    audit_log_action(db, user->user_id, "CANCEL_APPOINTMENT", "APPOINTMENT", id, "Cancelled by user");
    response_send_success("Appointment cancelled successfully", NULL, NULL);
}

