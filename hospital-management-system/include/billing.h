#ifndef BILLING_H
#define BILLING_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void handle_invoices_get(sqlite3 *db, const AuthUser *user, const char *query_string);
void handle_invoices_post(sqlite3 *db, const AuthUser *user, cJSON *body);
void handle_payments_post(sqlite3 *db, const AuthUser *user, cJSON *body);

#endif // BILLING_H

