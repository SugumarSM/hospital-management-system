#ifndef SESSION_H
#define SESSION_H

#include "sqlite3.h"

typedef struct {
    int user_id;
    char username[64];
    char role[32];
    char email[128];
} AuthUser;

int session_create(sqlite3 *db, int user_id, char *out_token);
int session_validate(sqlite3 *db, const char *token, AuthUser *out_user);
int session_destroy(sqlite3 *db, const char *token);
void session_cleanup_expired(sqlite3 *db);

#endif // SESSION_H

