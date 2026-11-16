#include "session.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

static void generate_secure_token(char *token, size_t hex_len) {
    size_t byte_count = hex_len / 2;
    unsigned char *bytes = (unsigned char *)malloc(byte_count);
    int generated = 0;

#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptGenRandom(hCryptProv, (DWORD)byte_count, bytes)) {
            generated = 1;
        }
        CryptReleaseContext(hCryptProv, 0);
    }
#endif

    if (!generated) {
        srand((unsigned int)time(NULL) ^ 0x55AA);
        for (size_t i = 0; i < byte_count; i++) {
            bytes[i] = (unsigned char)(rand() % 256);
        }
    }

    for (size_t i = 0; i < byte_count; i++) {
        sprintf(token + (i * 2), "%02x", bytes[i]);
    }
    token[hex_len] = '\0';
    free(bytes);
}

int session_create(sqlite3 *db, int user_id, char *out_token) {
    if (!db || user_id <= 0 || !out_token) return -1;
    generate_secure_token(out_token, 48);

    long now = (long)time(NULL);
    long expires_at = now + SESSION_TTL_SECONDS;

    const char *sql = "INSERT INTO sessions (session_token, user_id, expires_at) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, out_token, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int64(stmt, 3, expires_at);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int session_validate(sqlite3 *db, const char *token, AuthUser *out_user) {
    if (!db || !token || strlen(token) == 0 || !out_user) return 0;

    long now = (long)time(NULL);
    const char *sql = "SELECT u.id, u.username, u.role, u.email "
                      "FROM sessions s "
                      "JOIN users u ON s.user_id = u.id "
                      "WHERE s.session_token = ? AND s.expires_at > ? AND u.status = 'active'";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, now);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        out_user->user_id = sqlite3_column_int(stmt, 0);
        snprintf(out_user->username, sizeof(out_user->username), "%s", (const char*)sqlite3_column_text(stmt, 1));
        snprintf(out_user->role, sizeof(out_user->role), "%s", (const char*)sqlite3_column_text(stmt, 2));
        snprintf(out_user->email, sizeof(out_user->email), "%s", (const char*)sqlite3_column_text(stmt, 3));
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int session_destroy(sqlite3 *db, const char *token) {
    if (!db || !token) return -1;
    const char *sql = "DELETE FROM sessions WHERE session_token = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

void session_cleanup_expired(sqlite3 *db) {
    if (!db) return;
    long now = (long)time(NULL);
    const char *sql = "DELETE FROM sessions WHERE expires_at <= ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

