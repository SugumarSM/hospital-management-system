#ifndef PHARMACY_H
#define PHARMACY_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void handle_medicines_get(sqlite3 *db, const AuthUser *user, const char *query_string);
void handle_medicines_post(sqlite3 *db, const AuthUser *user, cJSON *body);
void handle_medicines_put(sqlite3 *db, const AuthUser *user, cJSON *body);

#endif // PHARMACY_H

