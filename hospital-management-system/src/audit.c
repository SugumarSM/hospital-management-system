#include "audit.h"
#include <stdio.h>

int audit_log_action(sqlite3 *db, int user_id, const char *action, const char *entity_type, int entity_id, const char *details) {
    if (!db || !action || !entity_type) return -1;
    const char *sql = "INSERT INTO audit_logs (user_id, action, entity_type, entity_id, details) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    if (user_id > 0) {
        sqlite3_bind_int(stmt, 1, user_id);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    sqlite3_bind_text(stmt, 2, action, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, entity_type, -1, SQLITE_STATIC);
    if (entity_id > 0) {
        sqlite3_bind_int(stmt, 4, entity_id);
    } else {
        sqlite3_bind_null(stmt, 4);
    }
    if (details) {
        sqlite3_bind_text(stmt, 5, details, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 5);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

