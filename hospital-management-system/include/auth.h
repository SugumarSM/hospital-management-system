#ifndef AUTH_H
#define AUTH_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void auth_generate_salt(char *out_salt, size_t len);
void auth_hash_password(const char *password, const char *salt, char *out_hash_entry);
int auth_verify_password(const char *password, const char *stored_hash_entry);
int auth_has_role(const AuthUser *user, const char *role);
int auth_is_medical_staff(const AuthUser *user);

void handle_auth_login(sqlite3 *db, cJSON *body);
void handle_auth_register(sqlite3 *db, cJSON *body);
void handle_auth_logout(sqlite3 *db, const char *token);
void handle_auth_me(sqlite3 *db, const AuthUser *user);

#endif // AUTH_H

