#ifndef DATABASE_H
#define DATABASE_H

#include "sqlite3.h"

int db_open(sqlite3 **db);
void db_close(sqlite3 *db);
int db_init_schema(sqlite3 *db, const char *schema_file);
int db_begin_transaction(sqlite3 *db);
int db_commit_transaction(sqlite3 *db);
int db_rollback_transaction(sqlite3 *db);

#endif // DATABASE_H

