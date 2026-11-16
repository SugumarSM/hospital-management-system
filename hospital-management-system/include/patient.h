#ifndef PATIENT_H
#define PATIENT_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void handle_patients_get(sqlite3 *db, const AuthUser *user, const char *query_string);
void handle_patients_post(sqlite3 *db, const AuthUser *user, cJSON *body);
void handle_patients_put(sqlite3 *db, const AuthUser *user, cJSON *body);
void handle_patients_delete(sqlite3 *db, const AuthUser *user, const char *query_string);

#endif // PATIENT_H

