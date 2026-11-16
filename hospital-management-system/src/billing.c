#include "billing.h"
#include "database.h"
#include "response.h"
#include "json_utils.h"
#include "validation.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void handle_invoices_get(sqlite3 *db, const AuthUser *user, const char *query_string) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }

    int patient_id = 0;
    int invoice_id = 0;
    char status[32] = "";

    if (query_string) {
        const char *p = strstr(query_string, "patient_id=");
        if (p) patient_id = atoi(p + 11);
        p = strstr(query_string, "id=");
        if (p) invoice_id = atoi(p + 3);
        p = strstr(query_string, "status=");
        if (p) {
            const char *val = p + 7;
            const char *end = strchr(val, '&');
            size_t len = end ? (size_t)(end - val) : strlen(val);
            if (len >= sizeof(status)) len = sizeof(status) - 1;
            strncpy(status, val, len);
            status[len] = '\0';
        }
    }

    // Role restrictions: Patients can only view their own invoices
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
            response_send_success("Invoices retrieved", cJSON_CreateArray(), NULL);
            return;
        }
    }

    char sql[1024] = "SELECT inv.id, inv.patient_id, p.full_name as patient_name, p.patient_code, p.phone as patient_phone, "
                     "inv.appointment_id, a.appointment_date, d.full_name as doctor_name, "
                     "inv.subtotal, inv.tax, inv.total_amount, inv.payment_status, inv.created_at "
                     "FROM invoices inv "
                     "JOIN patients p ON inv.patient_id = p.id "
                     "LEFT JOIN appointments a ON inv.appointment_id = a.id "
                     "LEFT JOIN doctors d ON a.doctor_id = d.id "
                     "WHERE 1=1";

    if (invoice_id > 0) strcat(sql, " AND inv.id = ?");
    if (patient_id > 0) strcat(sql, " AND inv.patient_id = ?");
    if (strlen(status) > 0) strcat(sql, " AND inv.payment_status = ?");
    strcat(sql, " ORDER BY inv.id DESC LIMIT 150");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    int b_idx = 1;
    if (invoice_id > 0) sqlite3_bind_int(stmt, b_idx++, invoice_id);
    if (patient_id > 0) sqlite3_bind_int(stmt, b_idx++, patient_id);
    if (strlen(status) > 0) sqlite3_bind_text(stmt, b_idx++, status, -1, SQLITE_STATIC);

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        int cur_inv_id = sqlite3_column_int(stmt, 0);
        cJSON_AddNumberToObject(row, "id", cur_inv_id);
        cJSON_AddNumberToObject(row, "patient_id", sqlite3_column_int(stmt, 1));
        cJSON_AddStringToObject(row, "patient_name", (const char*)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(row, "patient_code", (const char*)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(row, "patient_phone", (const char*)sqlite3_column_text(stmt, 4));
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            cJSON_AddNumberToObject(row, "appointment_id", sqlite3_column_int(stmt, 5));
        } else {
            cJSON_AddNullToObject(row, "appointment_id");
        }
        cJSON_AddStringToObject(row, "appointment_date", sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "N/A");
        cJSON_AddStringToObject(row, "doctor_name", sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "Hospital Service");
        cJSON_AddNumberToObject(row, "subtotal", sqlite3_column_double(stmt, 8));
        cJSON_AddNumberToObject(row, "tax", sqlite3_column_double(stmt, 9));
        cJSON_AddNumberToObject(row, "total_amount", sqlite3_column_double(stmt, 10));
        cJSON_AddStringToObject(row, "payment_status", (const char*)sqlite3_column_text(stmt, 11));
        cJSON_AddStringToObject(row, "created_at", (const char*)sqlite3_column_text(stmt, 12));

        // Fetch payments made towards this invoice
        cJSON *pays_arr = cJSON_CreateArray();
        sqlite3_stmt *pstmt = NULL;
        const char *psql = "SELECT id, amount, payment_method, payment_date, reference_number FROM payments WHERE invoice_id = ?";
        if (sqlite3_prepare_v2(db, psql, -1, &pstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(pstmt, 1, cur_inv_id);
            while (sqlite3_step(pstmt) == SQLITE_ROW) {
                cJSON *p_item = cJSON_CreateObject();
                cJSON_AddNumberToObject(p_item, "id", sqlite3_column_int(pstmt, 0));
                cJSON_AddNumberToObject(p_item, "amount", sqlite3_column_double(pstmt, 1));
                cJSON_AddStringToObject(p_item, "payment_method", (const char*)sqlite3_column_text(pstmt, 2));
                cJSON_AddStringToObject(p_item, "payment_date", (const char*)sqlite3_column_text(pstmt, 3));
                cJSON_AddStringToObject(p_item, "reference_number", (const char*)sqlite3_column_text(pstmt, 4));
                cJSON_AddItemToArray(pays_arr, p_item);
            }
            sqlite3_finalize(pstmt);
        }
        cJSON_AddItemToObject(row, "payments", pays_arr);

        cJSON_AddItemToArray(arr, row);
    }
    sqlite3_finalize(stmt);

    response_send_success("Invoices retrieved successfully", arr, NULL);
}

void handle_invoices_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "receptionist") != 0) {
        response_send_error(403, "Only billing staff or administrators can generate invoices");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int patient_id = json_get_int(body, "patient_id", 0);
    int appointment_id = json_get_int(body, "appointment_id", 0);
    double subtotal = json_get_double(body, "subtotal", 0.0);

    if (patient_id <= 0) {
        response_send_error(400, "Valid patient ID is required");
        return;
    }
    if (subtotal <= 0.0) {
        response_send_error(400, "Subtotal must be greater than zero");
        return;
    }

    double tax = subtotal * 0.05; // 5% GST
    double total_amount = subtotal + tax;

    const char *sql = "INSERT INTO invoices (patient_id, appointment_id, subtotal, tax, total_amount, payment_status) "
                      "VALUES (?, ?, ?, ?, ?, 'Unpaid')";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_int(stmt, 1, patient_id);
    if (appointment_id > 0) sqlite3_bind_int(stmt, 2, appointment_id); else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_double(stmt, 3, subtotal);
    sqlite3_bind_double(stmt, 4, tax);
    sqlite3_bind_double(stmt, 5, total_amount);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to create invoice");
        return;
    }
    sqlite3_finalize(stmt);

    int new_inv_id = (int)sqlite3_last_insert_rowid(db);
    audit_log_action(db, user->user_id, "CREATE_INVOICE", "INVOICE", new_inv_id, "Invoice generated");

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "invoice_id", new_inv_id);
    cJSON_AddNumberToObject(data, "total_amount", total_amount);

    response_send_success("Invoice created successfully", data, NULL);
}

void handle_payments_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int invoice_id = json_get_int(body, "invoice_id", 0);
    double amount = json_get_double(body, "amount", 0.0);
    const char *method = json_get_string(body, "payment_method", "Cash");

    if (invoice_id <= 0) {
        response_send_error(400, "Valid invoice ID is required");
        return;
    }
    if (amount <= 0.0) {
        response_send_error(400, "Payment amount must be greater than zero");
        return;
    }

    // Check invoice and current total
    sqlite3_stmt *inv_stmt = NULL;
    double total_amount = 0.0;
    if (sqlite3_prepare_v2(db, "SELECT total_amount FROM invoices WHERE id = ?", -1, &inv_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(inv_stmt, 1, invoice_id);
        if (sqlite3_step(inv_stmt) == SQLITE_ROW) {
            total_amount = sqlite3_column_double(inv_stmt, 0);
        } else {
            sqlite3_finalize(inv_stmt);
            response_send_error(404, "Invoice not found");
            return;
        }
        sqlite3_finalize(inv_stmt);
    }

    // Begin transaction for safe payment processing
    db_begin_transaction(db);

    char ref_no[64];
    snprintf(ref_no, sizeof(ref_no), "PAY-%s-%ld-%d", method, (long)time(NULL), rand() % 1000);

    const char *psql = "INSERT INTO payments (invoice_id, amount, payment_method, reference_number) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *pstmt = NULL;
    if (sqlite3_prepare_v2(db, psql, -1, &pstmt, NULL) != SQLITE_OK) {
        db_rollback_transaction(db);
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_int(pstmt, 1, invoice_id);
    sqlite3_bind_double(pstmt, 2, amount);
    sqlite3_bind_text(pstmt, 3, method, -1, SQLITE_STATIC);
    sqlite3_bind_text(pstmt, 4, ref_no, -1, SQLITE_STATIC);

    if (sqlite3_step(pstmt) != SQLITE_DONE) {
        sqlite3_finalize(pstmt);
        db_rollback_transaction(db);
        response_send_error(500, "Failed to record payment");
        return;
    }
    sqlite3_finalize(pstmt);

    // Calculate total paid so far
    double total_paid = 0.0;
    sqlite3_stmt *sum_stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(SUM(amount), 0.0) FROM payments WHERE invoice_id = ?", -1, &sum_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(sum_stmt, 1, invoice_id);
        if (sqlite3_step(sum_stmt) == SQLITE_ROW) {
            total_paid = sqlite3_column_double(sum_stmt, 0);
        }
        sqlite3_finalize(sum_stmt);
    }

    const char *new_status = (total_paid >= total_amount - 0.01) ? "Paid" : "Partially Paid";
    sqlite3_stmt *up_stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE invoices SET payment_status = ? WHERE id = ?", -1, &up_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up_stmt, 1, new_status, -1, SQLITE_STATIC);
        sqlite3_bind_int(up_stmt, 2, invoice_id);
        sqlite3_step(up_stmt);
        sqlite3_finalize(up_stmt);
    }

    db_commit_transaction(db);

    audit_log_action(db, user->user_id, "RECORD_PAYMENT", "PAYMENT", invoice_id, ref_no);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "reference_number", ref_no);
    cJSON_AddNumberToObject(data, "amount", amount);
    cJSON_AddStringToObject(data, "payment_status", new_status);

    response_send_success("Payment recorded successfully (Academic Demo Simulation)", data, NULL);
}

