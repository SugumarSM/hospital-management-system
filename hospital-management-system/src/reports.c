#include "reports.h"
#include "response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_reports_dashboard(sqlite3 *db, const AuthUser *user) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }

    cJSON *data = cJSON_CreateObject();

    // 1. Total patients
    int total_patients = 0;
    sqlite3_stmt *s1 = NULL;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM patients", -1, &s1, NULL) == SQLITE_OK) {
        if (sqlite3_step(s1) == SQLITE_ROW) total_patients = sqlite3_column_int(s1, 0);
        sqlite3_finalize(s1);
    }
    cJSON_AddNumberToObject(data, "total_patients", total_patients);

    // 2. Total doctors
    int total_doctors = 0;
    sqlite3_stmt *s2 = NULL;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM doctors WHERE status = 'active'", -1, &s2, NULL) == SQLITE_OK) {
        if (sqlite3_step(s2) == SQLITE_ROW) total_doctors = sqlite3_column_int(s2, 0);
        sqlite3_finalize(s2);
    }
    cJSON_AddNumberToObject(data, "total_doctors", total_doctors);

    // 3. Appointments stats & breakdown
    int total_appts = 0;
    int pending_appts = 0;
    int confirmed_appts = 0;
    int completed_appts = 0;
    int cancelled_appts = 0;

    sqlite3_stmt *s3 = NULL;
    const char *appt_sql = "SELECT status, count(*) FROM appointments GROUP BY status";
    if (sqlite3_prepare_v2(db, appt_sql, -1, &s3, NULL) == SQLITE_OK) {
        while (sqlite3_step(s3) == SQLITE_ROW) {
            const char *st = (const char*)sqlite3_column_text(s3, 0);
            int count = sqlite3_column_int(s3, 1);
            total_appts += count;
            if (strcmp(st, "Pending") == 0) pending_appts = count;
            else if (strcmp(st, "Confirmed") == 0) confirmed_appts = count;
            else if (strcmp(st, "Completed") == 0) completed_appts = count;
            else if (strcmp(st, "Cancelled") == 0) cancelled_appts = count;
        }
        sqlite3_finalize(s3);
    }
    cJSON_AddNumberToObject(data, "total_appointments", total_appts);
    cJSON_AddNumberToObject(data, "pending_appointments", pending_appts);
    cJSON_AddNumberToObject(data, "confirmed_appointments", confirmed_appts);
    cJSON_AddNumberToObject(data, "completed_appointments", completed_appts);
    cJSON_AddNumberToObject(data, "cancelled_appointments", cancelled_appts);

    double completion_rate = total_appts > 0 ? ((double)completed_appts / total_appts) * 100.0 : 0.0;
    cJSON_AddNumberToObject(data, "completion_rate", completion_rate);

    // 4. Total revenue in INR
    double total_revenue = 0.0;
    sqlite3_stmt *s4 = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(SUM(amount), 0.0) FROM payments", -1, &s4, NULL) == SQLITE_OK) {
        if (sqlite3_step(s4) == SQLITE_ROW) total_revenue = sqlite3_column_double(s4, 0);
        sqlite3_finalize(s4);
    }
    cJSON_AddNumberToObject(data, "total_revenue", total_revenue);
    cJSON_AddStringToObject(data, "currency", "INR (₹)");

    // 5. Emergency cases count
    int active_emergencies = 0;
    sqlite3_stmt *s5 = NULL;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM emergency_cases WHERE status != 'Discharged'", -1, &s5, NULL) == SQLITE_OK) {
        if (sqlite3_step(s5) == SQLITE_ROW) active_emergencies = sqlite3_column_int(s5, 0);
        sqlite3_finalize(s5);
    }
    cJSON_AddNumberToObject(data, "active_emergencies", active_emergencies);

    // 6. Low stock medicines
    int low_stock_count = 0;
    sqlite3_stmt *s6 = NULL;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM medicines WHERE quantity <= minimum_stock", -1, &s6, NULL) == SQLITE_OK) {
        if (sqlite3_step(s6) == SQLITE_ROW) low_stock_count = sqlite3_column_int(s6, 0);
        sqlite3_finalize(s6);
    }
    cJSON_AddNumberToObject(data, "low_stock_medicines", low_stock_count);

    // 7. Department appointments distribution (for Chart.js)
    cJSON *dept_chart = cJSON_CreateArray();
    sqlite3_stmt *s7 = NULL;
    const char *d_sql = "SELECT dep.department_name, count(a.id) FROM departments dep "
                        "LEFT JOIN doctors d ON dep.id = d.department_id "
                        "LEFT JOIN appointments a ON d.id = a.doctor_id "
                        "GROUP BY dep.id ORDER BY count(a.id) DESC";
    if (sqlite3_prepare_v2(db, d_sql, -1, &s7, NULL) == SQLITE_OK) {
        while (sqlite3_step(s7) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "department", (const char*)sqlite3_column_text(s7, 0));
            cJSON_AddNumberToObject(item, "count", sqlite3_column_int(s7, 1));
            cJSON_AddItemToArray(dept_chart, item);
        }
        sqlite3_finalize(s7);
    }
    cJSON_AddItemToObject(data, "department_distribution", dept_chart);

    // 8. Recent 7 days appointment trend (for Chart.js line chart)
    cJSON *trend_chart = cJSON_CreateArray();
    sqlite3_stmt *s8 = NULL;
    const char *t_sql = "SELECT appointment_date, count(*) FROM appointments GROUP BY appointment_date ORDER BY appointment_date DESC LIMIT 7";
    if (sqlite3_prepare_v2(db, t_sql, -1, &s8, NULL) == SQLITE_OK) {
        while (sqlite3_step(s8) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "date", (const char*)sqlite3_column_text(s8, 0));
            cJSON_AddNumberToObject(item, "count", sqlite3_column_int(s8, 1));
            cJSON_AddItemToArray(trend_chart, item);
        }
        sqlite3_finalize(s8);
    }
    cJSON_AddItemToObject(data, "appointment_trends", trend_chart);

    response_send_success("Dashboard analytics retrieved", data, NULL);
}

void handle_reports_revenue(sqlite3 *db, const AuthUser *user) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "receptionist") != 0) {
        response_send_error(403, "Access restricted to administrators and billing managers");
        return;
    }

    cJSON *data = cJSON_CreateObject();

    // Summary totals
    double total_revenue = 0.0;
    double unpaid_total = 0.0;

    sqlite3_stmt *s1 = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(SUM(amount), 0.0) FROM payments", -1, &s1, NULL) == SQLITE_OK) {
        if (sqlite3_step(s1) == SQLITE_ROW) total_revenue = sqlite3_column_double(s1, 0);
        sqlite3_finalize(s1);
    }
    sqlite3_stmt *s2 = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(SUM(total_amount), 0.0) FROM invoices WHERE payment_status = 'Unpaid'", -1, &s2, NULL) == SQLITE_OK) {
        if (sqlite3_step(s2) == SQLITE_ROW) unpaid_total = sqlite3_column_double(s2, 0);
        sqlite3_finalize(s2);
    }

    cJSON_AddNumberToObject(data, "total_revenue_collected", total_revenue);
    cJSON_AddNumberToObject(data, "total_pending_receivables", unpaid_total);
    cJSON_AddStringToObject(data, "currency", "INR (₹)");

    // Revenue by payment method
    cJSON *method_arr = cJSON_CreateArray();
    sqlite3_stmt *s3 = NULL;
    const char *m_sql = "SELECT payment_method, count(*), sum(amount) FROM payments GROUP BY payment_method";
    if (sqlite3_prepare_v2(db, m_sql, -1, &s3, NULL) == SQLITE_OK) {
        while (sqlite3_step(s3) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "method", (const char*)sqlite3_column_text(s3, 0));
            cJSON_AddNumberToObject(item, "count", sqlite3_column_int(s3, 1));
            cJSON_AddNumberToObject(item, "amount", sqlite3_column_double(s3, 2));
            cJSON_AddItemToArray(method_arr, item);
        }
        sqlite3_finalize(s3);
    }
    cJSON_AddItemToObject(data, "by_payment_method", method_arr);

    // Revenue by department
    cJSON *dept_rev = cJSON_CreateArray();
    sqlite3_stmt *s4 = NULL;
    const char *dr_sql = "SELECT dep.department_name, COALESCE(SUM(p.amount), 0.0) "
                         "FROM departments dep "
                         "JOIN doctors d ON dep.id = d.department_id "
                         "JOIN appointments a ON d.id = a.doctor_id "
                         "JOIN invoices inv ON a.id = inv.appointment_id "
                         "JOIN payments p ON inv.id = p.invoice_id "
                         "GROUP BY dep.id";
    if (sqlite3_prepare_v2(db, dr_sql, -1, &s4, NULL) == SQLITE_OK) {
        while (sqlite3_step(s4) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "department", (const char*)sqlite3_column_text(s4, 0));
            cJSON_AddNumberToObject(item, "revenue", sqlite3_column_double(s4, 1));
            cJSON_AddItemToArray(dept_rev, item);
        }
        sqlite3_finalize(s4);
    }
    cJSON_AddItemToObject(data, "by_department", dept_rev);

    response_send_success("Revenue analytics retrieved", data, NULL);
}

