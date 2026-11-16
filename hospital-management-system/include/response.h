#ifndef RESPONSE_H
#define RESPONSE_H

#include "cJSON.h"

void response_send_cors_headers(void);
void response_send_json(int status_code, cJSON *json_obj, const char *set_cookie);
void response_send_success(const char *message, cJSON *data, const char *set_cookie);
void response_send_error(int status_code, const char *message);

#endif // RESPONSE_H

