#ifndef MEDICAL_RECORDS_H
#define MEDICAL_RECORDS_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void handle_medical_records_get(sqlite3 *db, const AuthUser *user, const char *query_string);
void handle_medical_records_post(sqlite3 *db, const AuthUser *user, cJSON *body);

#endif // MEDICAL_RECORDS_H

