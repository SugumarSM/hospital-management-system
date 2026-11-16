#ifndef REPORTS_H
#define REPORTS_H

#include "sqlite3.h"
#include "cJSON.h"
#include "session.h"

void handle_reports_dashboard(sqlite3 *db, const AuthUser *user);
void handle_reports_revenue(sqlite3 *db, const AuthUser *user);

#endif // REPORTS_H

