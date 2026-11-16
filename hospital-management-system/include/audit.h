#ifndef AUDIT_H
#define AUDIT_H

#include "sqlite3.h"

int audit_log_action(sqlite3 *db, int user_id, const char *action, const char *entity_type, int entity_id, const char *details);

#endif // AUDIT_H

