#ifndef EMERGENCY_H
#define EMERGENCY_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void handle_emergency_get(sqlite3 *db, const AuthUser *user, const char *query_string);
void handle_emergency_post(sqlite3 *db, const AuthUser *user, cJSON *body);
void handle_emergency_put(sqlite3 *db, const AuthUser *user, cJSON *body);

#endif // EMERGENCY_H

