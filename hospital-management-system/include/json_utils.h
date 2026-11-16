#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "cJSON.h"

const char* json_get_string(cJSON *root, const char *key, const char *default_val);
int json_get_int(cJSON *root, const char *key, int default_val);
double json_get_double(cJSON *root, const char *key, double default_val);

#endif // JSON_UTILS_H

