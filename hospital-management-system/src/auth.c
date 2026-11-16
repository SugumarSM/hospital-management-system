#include "auth.h"
#include "session.h"
#include "response.h"
#include "json_utils.h"
#include "validation.h"
#include "audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

// Constant-time memory compare to prevent timing side-channel attacks
static int safe_memequal(const void *b1, const void *b2, size_t len) {
    const unsigned char *p1 = (const unsigned char *)b1;
    const unsigned char *p2 = (const unsigned char *)b2;
    unsigned char result = 0;
    for (size_t i = 0; i < len; i++) {
        result |= (p1[i] ^ p2[i]);
    }
    return (result == 0);
}

static void compute_sha256_hex(const char *input, char out_hex[65]) {
    out_hex[0] = '\0';
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        HCRYPTHASH hHash = 0;
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptHashData(hHash, (const BYTE*)input, (DWORD)strlen(input), 0);
            BYTE hash_bytes[32];
            DWORD hash_len = 32;
            if (CryptGetHashParam(hHash, HP_HASHVAL, hash_bytes, &hash_len, 0)) {
                for (int i = 0; i < 32; i++) {
                    sprintf(out_hex + (i * 2), "%02x", hash_bytes[i]);
                }
                out_hex[64] = '\0';
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
#endif
}

void auth_generate_salt(char *out_salt, size_t len) {
    static const char hex_chars[] = "0123456789abcdef";
    srand((unsigned int)time(NULL) ^ 0xA55A);
    for (size_t i = 0; i < len; i++) {
        out_salt[i] = hex_chars[rand() % 16];
    }
    out_salt[len] = '\0';
}

void auth_hash_password(const char *password, const char *salt, char *out_hash_entry) {
    char combined[512];
    snprintf(combined, sizeof(combined), "%s%s", salt, password);

    char hex[65];
    compute_sha256_hex(combined, hex);

    sprintf(out_hash_entry, "sha256$%s$%s", salt, hex);
}

int auth_verify_password(const char *password, const char *stored_hash_entry) {
    if (!password || !stored_hash_entry) return 0;
    if (strncmp(stored_hash_entry, "sha256$", 7) != 0) return 0;

    const char *salt_start = stored_hash_entry + 7;
    const char *hash_delim = strchr(salt_start, '$');
    if (!hash_delim) return 0;

    size_t salt_len = hash_delim - salt_start;
    if (salt_len >= 64) return 0;

    char salt[64];
    strncpy(salt, salt_start, salt_len);
    salt[salt_len] = '\0';

    const char *expected_hex = hash_delim + 1;

    char combined[512];
    snprintf(combined, sizeof(combined), "%s%s", salt, password);

    char computed_hex[65];
    compute_sha256_hex(combined, computed_hex);

    if (strlen(computed_hex) != strlen(expected_hex)) return 0;
    return safe_memequal(computed_hex, expected_hex, strlen(expected_hex));
}

int auth_has_role(const AuthUser *user, const char *role) {
    if (!user || !role) return 0;
    // Admin has superuser privileges
    if (strcmp(user->role, "admin") == 0) return 1;
    return (strcmp(user->role, role) == 0);
}

int auth_is_medical_staff(const AuthUser *user) {
    if (!user) return 0;
    return (strcmp(user->role, "admin") == 0 || strcmp(user->role, "doctor") == 0);
}

void handle_auth_login(sqlite3 *db, cJSON *body) {
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    const char *username = json_get_string(body, "username", NULL);
    const char *password = json_get_string(body, "password", NULL);

    if (!validate_not_empty(username) || !validate_not_empty(password)) {
        response_send_error(400, "Username and password are required");
        return;
    }

    const char *sql = "SELECT id, username, email, password_hash, role, status FROM users WHERE username = ? COLLATE NOCASE";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Database query preparation failed");
        return;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        response_send_error(401, "Invalid username or password");
        return;
    }

    int user_id = sqlite3_column_int(stmt, 0);
    const char *uname = (const char*)sqlite3_column_text(stmt, 1);
    const char *email = (const char*)sqlite3_column_text(stmt, 2);
    const char *pwd_hash = (const char*)sqlite3_column_text(stmt, 3);
    const char *role = (const char*)sqlite3_column_text(stmt, 4);
    const char *status = (const char*)sqlite3_column_text(stmt, 5);

    if (strcmp(status, "active") != 0) {
        sqlite3_finalize(stmt);
        response_send_error(403, "Account is deactivated. Please contact administration.");
        return;
    }

    if (!auth_verify_password(password, pwd_hash)) {
        sqlite3_finalize(stmt);
        audit_log_action(db, user_id, "FAILED_LOGIN", "USER", user_id, "Incorrect password attempt");
        response_send_error(401, "Invalid username or password");
        return;
    }

    char token[64];
    if (session_create(db, user_id, token) != 0) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Could not establish secure session");
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "user_id", user_id);
    cJSON_AddStringToObject(data, "username", uname);
    cJSON_AddStringToObject(data, "email", email ? email : "");
    cJSON_AddStringToObject(data, "role", role);
    cJSON_AddStringToObject(data, "token", token);

    // If patient or doctor, also include specific entity ID
    if (strcmp(role, "patient") == 0) {
        sqlite3_stmt *p_stmt = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id, patient_code, full_name FROM patients WHERE user_id = ?", -1, &p_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(p_stmt, 1, user_id);
            if (sqlite3_step(p_stmt) == SQLITE_ROW) {
                cJSON_AddNumberToObject(data, "patient_id", sqlite3_column_int(p_stmt, 0));
                cJSON_AddStringToObject(data, "patient_code", (const char*)sqlite3_column_text(p_stmt, 1));
                cJSON_AddStringToObject(data, "full_name", (const char*)sqlite3_column_text(p_stmt, 2));
            }
            sqlite3_finalize(p_stmt);
        }
    } else if (strcmp(role, "doctor") == 0) {
        sqlite3_stmt *d_stmt = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id, doctor_code, full_name, specialization FROM doctors WHERE user_id = ?", -1, &d_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(d_stmt, 1, user_id);
            if (sqlite3_step(d_stmt) == SQLITE_ROW) {
                cJSON_AddNumberToObject(data, "doctor_id", sqlite3_column_int(d_stmt, 0));
                cJSON_AddStringToObject(data, "doctor_code", (const char*)sqlite3_column_text(d_stmt, 1));
                cJSON_AddStringToObject(data, "full_name", (const char*)sqlite3_column_text(d_stmt, 2));
                cJSON_AddStringToObject(data, "specialization", (const char*)sqlite3_column_text(d_stmt, 3));
            }
            sqlite3_finalize(d_stmt);
        }
    }

    sqlite3_finalize(stmt);

    audit_log_action(db, user_id, "LOGIN_SUCCESS", "USER", user_id, "User logged in successfully");

    char cookie_header[128];
    snprintf(cookie_header, sizeof(cookie_header), "session_id=%s", token);
    response_send_success("Login successful", data, cookie_header);
}

void handle_auth_register(sqlite3 *db, cJSON *body) {
    if (!body) {
        response_send_error(400, "Missing request body");
        return;
    }

    const char *username = json_get_string(body, "username", NULL);
    const char *email = json_get_string(body, "email", NULL);
    const char *password = json_get_string(body, "password", NULL);
    const char *full_name = json_get_string(body, "full_name", NULL);
    const char *phone = json_get_string(body, "phone", NULL);
    const char *dob = json_get_string(body, "date_of_birth", "1990-01-01");
    const char *gender = json_get_string(body, "gender", "Male");
    const char *address = json_get_string(body, "address", "Not provided");
    const char *emergency_contact = json_get_string(body, "emergency_contact", "Not provided");
    const char *blood_group = json_get_string(body, "blood_group", "O+");
    const char *role = json_get_string(body, "role", "patient");

    if (!validate_not_empty(username) || strlen(username) < 3) {
        response_send_error(400, "Username must be at least 3 characters");
        return;
    }
    if (!validate_email(email)) {
        response_send_error(400, "Valid email address is required");
        return;
    }
    if (!validate_not_empty(password) || strlen(password) < 6) {
        response_send_error(400, "Password must be at least 6 characters");
        return;
    }
    if (!validate_not_empty(full_name)) {
        response_send_error(400, "Full name is required");
        return;
    }

    // Check unique username and email
    sqlite3_stmt *chk = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id FROM users WHERE username = ? COLLATE NOCASE OR email = ? COLLATE NOCASE", -1, &chk, NULL) == SQLITE_OK) {
        sqlite3_bind_text(chk, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(chk, 2, email, -1, SQLITE_STATIC);
        if (sqlite3_step(chk) == SQLITE_ROW) {
            sqlite3_finalize(chk);
            response_send_error(409, "Username or email is already registered");
            return;
        }
        sqlite3_finalize(chk);
    }

    char salt[17];
    auth_generate_salt(salt, 16);
    char hash_entry[256];
    auth_hash_password(password, salt, hash_entry);

    const char *user_sql = "INSERT INTO users (username, email, password_hash, role, status) VALUES (?, ?, ?, ?, 'active')";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, user_sql, -1, &stmt, NULL) != SQLITE_OK) {
        response_send_error(500, "Failed to prepare user insertion statement");
        return;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, hash_entry, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, role, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        response_send_error(500, "Failed to create user account");
        return;
    }
    sqlite3_finalize(stmt);

    int new_user_id = (int)sqlite3_last_insert_rowid(db);

    // If role is patient, automatically create patient profile
    if (strcmp(role, "patient") == 0) {
        char patient_code[32];
        snprintf(patient_code, sizeof(patient_code), "PAT-%04d", 1000 + new_user_id);

        const char *p_sql = "INSERT INTO patients (patient_code, user_id, full_name, date_of_birth, gender, phone, address, emergency_contact, blood_group) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
        sqlite3_stmt *pstmt = NULL;
        if (sqlite3_prepare_v2(db, p_sql, -1, &pstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(pstmt, 1, patient_code, -1, SQLITE_STATIC);
            sqlite3_bind_int(pstmt, 2, new_user_id);
            sqlite3_bind_text(pstmt, 3, full_name, -1, SQLITE_STATIC);
            sqlite3_bind_text(pstmt, 4, dob, -1, SQLITE_STATIC);
            sqlite3_bind_text(pstmt, 5, gender, -1, SQLITE_STATIC);
            sqlite3_bind_text(pstmt, 6, phone ? phone : "", -1, SQLITE_STATIC);
            sqlite3_bind_text(pstmt, 7, address ? address : "", -1, SQLITE_STATIC);
            sqlite3_bind_text(pstmt, 8, emergency_contact ? emergency_contact : "", -1, SQLITE_STATIC);
            sqlite3_bind_text(pstmt, 9, blood_group ? blood_group : "O+", -1, SQLITE_STATIC);
            sqlite3_step(pstmt);
            sqlite3_finalize(pstmt);
        }
    }

    audit_log_action(db, new_user_id, "REGISTER", "USER", new_user_id, "New patient registered");

    cJSON *resp_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp_data, "user_id", new_user_id);
    cJSON_AddStringToObject(resp_data, "username", username);
    cJSON_AddStringToObject(resp_data, "role", role);

    response_send_success("Registration successful. You can now login.", resp_data, NULL);
}

void handle_auth_logout(sqlite3 *db, const char *token) {
    if (token && strlen(token) > 0) {
        session_destroy(db, token);
    }
    // Expire the cookie on the browser
    response_send_success("Logged out successfully", NULL, "session_id=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
}

void handle_auth_me(sqlite3 *db, const AuthUser *user) {
    if (!user || user->user_id <= 0) {
        response_send_error(401, "Not authenticated");
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "user_id", user->user_id);
    cJSON_AddStringToObject(data, "username", user->username);
    cJSON_AddStringToObject(data, "email", user->email);
    cJSON_AddStringToObject(data, "role", user->role);

    if (strcmp(user->role, "patient") == 0) {
        sqlite3_stmt *stmt = NULL;
        const char *sql = "SELECT id, patient_code, full_name, date_of_birth, gender, phone, address, emergency_contact, blood_group FROM patients WHERE user_id = ?";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user->user_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                cJSON_AddNumberToObject(data, "patient_id", sqlite3_column_int(stmt, 0));
                cJSON_AddStringToObject(data, "patient_code", (const char*)sqlite3_column_text(stmt, 1));
                cJSON_AddStringToObject(data, "full_name", (const char*)sqlite3_column_text(stmt, 2));
                cJSON_AddStringToObject(data, "date_of_birth", (const char*)sqlite3_column_text(stmt, 3));
                cJSON_AddStringToObject(data, "gender", (const char*)sqlite3_column_text(stmt, 4));
                cJSON_AddStringToObject(data, "phone", (const char*)sqlite3_column_text(stmt, 5));
                cJSON_AddStringToObject(data, "address", (const char*)sqlite3_column_text(stmt, 6));
                cJSON_AddStringToObject(data, "emergency_contact", (const char*)sqlite3_column_text(stmt, 7));
                cJSON_AddStringToObject(data, "blood_group", (const char*)sqlite3_column_text(stmt, 8));
            }
            sqlite3_finalize(stmt);
        }
    } else if (strcmp(user->role, "doctor") == 0) {
        sqlite3_stmt *stmt = NULL;
        const char *sql = "SELECT d.id, d.doctor_code, d.full_name, dep.department_name, d.specialization, d.qualification, d.consultation_fee, d.availability "
                          "FROM doctors d JOIN departments dep ON d.department_id = dep.id WHERE d.user_id = ?";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user->user_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                cJSON_AddNumberToObject(data, "doctor_id", sqlite3_column_int(stmt, 0));
                cJSON_AddStringToObject(data, "doctor_code", (const char*)sqlite3_column_text(stmt, 1));
                cJSON_AddStringToObject(data, "full_name", (const char*)sqlite3_column_text(stmt, 2));
                cJSON_AddStringToObject(data, "department_name", (const char*)sqlite3_column_text(stmt, 3));
                cJSON_AddStringToObject(data, "specialization", (const char*)sqlite3_column_text(stmt, 4));
                cJSON_AddStringToObject(data, "qualification", (const char*)sqlite3_column_text(stmt, 5));
                cJSON_AddNumberToObject(data, "consultation_fee", sqlite3_column_double(stmt, 6));
                cJSON_AddStringToObject(data, "availability", (const char*)sqlite3_column_text(stmt, 7));
            }
            sqlite3_finalize(stmt);
        }
    }

    response_send_success("Current session details", data, NULL);
}

