#include "database.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

int db_open(sqlite3 **db) {
    const char *db_path = config_get_db_path();
    int rc = sqlite3_open(db_path, db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database '%s': %s\n", db_path, sqlite3_errmsg(*db));
        return rc;
    }
    // Enforce foreign keys as required by specification
    char *err_msg = NULL;
    rc = sqlite3_exec(*db, "PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            fprintf(stderr, "SQLite PRAGMA error: %s\n", err_msg);
            sqlite3_free(err_msg);
        }
    }
    return SQLITE_OK;
}

void db_close(sqlite3 *db) {
    if (db) {
        sqlite3_close(db);
    }
}

int db_begin_transaction(sqlite3 *db) {
    if (!db) return SQLITE_ERROR;
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK && err_msg) {
        sqlite3_free(err_msg);
    }
    return rc;
}

int db_commit_transaction(sqlite3 *db) {
    if (!db) return SQLITE_ERROR;
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK && err_msg) {
        sqlite3_free(err_msg);
    }
    return rc;
}

int db_rollback_transaction(sqlite3 *db) {
    if (!db) return SQLITE_ERROR;
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK && err_msg) {
        sqlite3_free(err_msg);
    }
    return rc;
}

