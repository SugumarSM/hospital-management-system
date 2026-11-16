#ifndef DOCTOR_H
#define DOCTOR_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void handle_doctors_get(sqlite3 *db, const AuthUser *user, const char *query_string);
void handle_doctors_post(sqlite3 *db, const AuthUser *user, cJSON *body);
void handle_doctors_put(sqlite3 *db, const AuthUser *user, cJSON *body);
void handle_departments_get(sqlite3 *db);
void handle_departments_post(sqlite3 *db, const AuthUser *user, cJSON *body);

#endif // DOCTOR_H

