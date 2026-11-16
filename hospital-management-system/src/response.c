#include "response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void response_send_cors_headers(void) {
    printf("Access-Control-Allow-Origin: *\r\n");
    printf("Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n");
    printf("Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, X-Session-Token\r\n");
    printf("Access-Control-Allow-Credentials: true\r\n");
}

void response_send_json(int status_code, cJSON *json_obj, const char *set_cookie) {
    printf("Status: %d\r\n", status_code);
    printf("Content-Type: application/json; charset=utf-8\r\n");
    response_send_cors_headers();
    if (set_cookie && strlen(set_cookie) > 0) {
        printf("Set-Cookie: %s; Path=/; HttpOnly; SameSite=Lax\r\n", set_cookie);
    }
    printf("\r\n");

    if (json_obj) {
        char *rendered = cJSON_PrintUnformatted(json_obj);
        if (rendered) {
            printf("%s\n", rendered);
            free(rendered);
        }
        cJSON_Delete(json_obj);
    } else {
        printf("{}\n");
    }
    fflush(stdout);
}

void response_send_success(const char *message, cJSON *data, const char *set_cookie) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 1);
    cJSON_AddStringToObject(root, "message", message ? message : "Success");
    if (data) {
        cJSON_AddItemToObject(root, "data", data);
    } else {
        cJSON_AddNullToObject(root, "data");
    }
    response_send_json(200, root, set_cookie);
}

void response_send_error(int status_code, const char *message) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 0);
    cJSON_AddStringToObject(root, "message", message ? message : "An error occurred");
    cJSON_AddNullToObject(root, "data");
    response_send_json(status_code, root, NULL);
}

