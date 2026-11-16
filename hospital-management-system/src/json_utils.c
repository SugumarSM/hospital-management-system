#include "json_utils.h"
#include <string.h>

const char* json_get_string(cJSON *root, const char *key, const char *default_val) {
    if (!root || !key) return default_val;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (item && cJSON_IsString(item) && item->valuestring) {
        return item->valuestring;
    }
    return default_val;
}

int json_get_int(cJSON *root, const char *key, int default_val) {
    if (!root || !key) return default_val;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (item && cJSON_IsNumber(item)) {
        return item->valueint;
    }
    return default_val;
}

double json_get_double(cJSON *root, const char *key, double default_val) {
    if (!root || !key) return default_val;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (item && cJSON_IsNumber(item)) {
        return item->valuedouble;
    }
    return default_val;
}

