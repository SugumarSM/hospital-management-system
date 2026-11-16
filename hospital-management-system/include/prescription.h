#ifndef PRESCRIPTION_H
#define PRESCRIPTION_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void handle_prescriptions_get(sqlite3 *db, const AuthUser *user, const char *query_string);
void handle_prescriptions_post(sqlite3 *db, const AuthUser *user, cJSON *body);

#endif // PRESCRIPTION_H

