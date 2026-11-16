#ifndef APPOINTMENT_H
#define APPOINTMENT_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void handle_appointments_get(sqlite3 *db, const AuthUser *user, const char *query_string);
void handle_appointments_post(sqlite3 *db, const AuthUser *user, cJSON *body);
void handle_appointments_put(sqlite3 *db, const AuthUser *user, cJSON *body);
void handle_appointments_cancel(sqlite3 *db, const AuthUser *user, cJSON *body);

#endif // APPOINTMENT_H

