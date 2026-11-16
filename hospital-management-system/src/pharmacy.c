#include "pharmacy.h"
#include "response.h"
#include "json_utils.h"
#include "validation.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_medicines_get(sqlite3 *db, const AuthUser *user, const char *query_string) {
    (void)user;
    int low_stock_only = 0;
    char search[128] = "";

    if (query_string) {
        if (strstr(query_string, "low_stock=true")) low_stock_only = 1;
        const char *p = strstr(query_string, "search=");
        if (p) {
            const char *val = p + 7;
            const char *end = strchr(val, '&');
            size_t len = end ? (size_t)(end - val) : strlen(val);
            if (len >= sizeof(search)) len = sizeof(search) - 1;
            strncpy(search, val, len);
            search[len] = '\0';
        }
    }

    char sql[512] = "SELECT id, medicine_name, category, quantity, unit_price, expiry_date, minimum_stock FROM medicines WHERE 1=1";
    if (low_stock_only) {
        strcat(sql, " AND quantity <= minimum_stock");
    }
    if (strlen(search) > 0) {
        strcat(sql, " AND (medicine_name LIKE ? OR category LIKE ?)");
    }
    strcat(sql, " ORDER BY quantity <= minimum_stock DESC, medicine_name ASC");

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
    }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        int qty = sqlite3_column_int(stmt, 3);
        int min_stock = sqlite3_column_int(stmt, 6);
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(row, "medicine_name", (const char*)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(row, "category", (const char*)sqlite3_column_text(stmt, 2));
        cJSON_AddNumberToObject(row, "quantity", qty);
        cJSON_AddNumberToObject(row, "unit_price", sqlite3_column_double(stmt, 4));
        cJSON_AddStringToObject(row, "expiry_date", (const char*)sqlite3_column_text(stmt, 5));
        cJSON_AddNumberToObject(row, "minimum_stock", min_stock);
        cJSON_AddBoolToObject(row, "is_low_stock", qty <= min_stock);
        cJSON_AddItemToArray(arr, row);
    }
    sqlite3_finalize(stmt);

    response_send_success("Pharmacy inventory retrieved", arr, NULL);
}

void handle_medicines_post(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "receptionist") != 0) {
        response_send_error(403, "Only administrators or pharmacy managers can add medicines");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    const char *name = json_get_string(body, "medicine_name", NULL);
    const char *category = json_get_string(body, "category", "General");
    int quantity = json_get_int(body, "quantity", 0);
    double unit_price = json_get_double(body, "unit_price", 0.0);
    const char *expiry = json_get_string(body, "expiry_date", "2027-12-31");
    int min_stock = json_get_int(body, "minimum_stock", 10);

    if (!validate_not_empty(name)) {
        response_send_error(400, "Medicine name is required");
        return;
    }
    if (unit_price < 0.0) {
        response_send_error(400, "Unit price cannot be negative");
        return;
    }

    const char *sql = "INSERT INTO medicines (medicine_name, category, quantity, unit_price, expiry_date, minimum_stock) "
                      "VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, category, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, quantity);
    sqlite3_bind_double(stmt, 4, unit_price);
    sqlite3_bind_text(stmt, 5, expiry, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, min_stock);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(409, "Medicine already exists or could not be added");
        return;
    }
    sqlite3_finalize(stmt);

    int new_id = (int)sqlite3_last_insert_rowid(db);
    audit_log_action(db, user->user_id, "ADD_MEDICINE", "MEDICINE", new_id, name);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "medicine_id", new_id);
    cJSON_AddStringToObject(data, "medicine_name", name);
    response_send_success("Medicine added to inventory", data, NULL);
}

void handle_medicines_put(sqlite3 *db, const AuthUser *user, cJSON *body) {
    if (!user) {
        response_send_error(401, "Authentication required");
        return;
    }
    if (strcmp(user->role, "admin") != 0 && strcmp(user->role, "receptionist") != 0) {
        response_send_error(403, "Insufficient permissions to update inventory");
        return;
    }
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    int id = json_get_int(body, "id", 0);
    if (id <= 0) {
        response_send_error(400, "Valid medicine ID is required");
        return;
    }

    const char *name = json_get_string(body, "medicine_name", NULL);
    const char *category = json_get_string(body, "category", NULL);
    const char *expiry = json_get_string(body, "expiry_date", NULL);
    cJSON *qty_item = cJSON_GetObjectItemCaseSensitive(body, "quantity");
    cJSON *price_item = cJSON_GetObjectItemCaseSensitive(body, "unit_price");
    cJSON *min_item = cJSON_GetObjectItemCaseSensitive(body, "minimum_stock");

    const char *sql = "UPDATE medicines SET "
                      "medicine_name = COALESCE(?, medicine_name), "
                      "category = COALESCE(?, category), "
                      "expiry_date = COALESCE(?, expiry_date), "
                      "quantity = CASE WHEN ? >= 0 THEN ? ELSE quantity END, "
                      "unit_price = CASE WHEN ? >= 0 THEN ? ELSE unit_price END, "
                      "minimum_stock = CASE WHEN ? >= 0 THEN ? ELSE minimum_stock END "
                      "WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database error");
        return;
    }

    if (name) sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 1);
    if (category) sqlite3_bind_text(stmt, 2, category, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 2);
    if (expiry) sqlite3_bind_text(stmt, 3, expiry, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 3);

    int qval = (qty_item && cJSON_IsNumber(qty_item)) ? qty_item->valueint : -1;
    sqlite3_bind_int(stmt, 4, qval);
    sqlite3_bind_int(stmt, 5, qval);

    double pval = (price_item && cJSON_IsNumber(price_item)) ? price_item->valuedouble : -1.0;
    sqlite3_bind_double(stmt, 6, pval);
    sqlite3_bind_double(stmt, 7, pval);

    int mval = (min_item && cJSON_IsNumber(min_item)) ? min_item->valueint : -1;
    sqlite3_bind_int(stmt, 8, mval);
    sqlite3_bind_int(stmt, 9, mval);

    sqlite3_bind_int(stmt, 10, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to update medicine");
        return;
    }
    sqlite3_finalize(stmt);

    audit_log_action(db, user->user_id, "UPDATE_MEDICINE", "MEDICINE", id, "Inventory updated");
    response_send_success("Medicine updated successfully", NULL, NULL);
}

