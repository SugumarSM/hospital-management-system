#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "config.h"
#include "database.h"
#include "response.h"
#include "json_utils.h"
#include "session.h"
#include "auth.h"
#include "patient.h"
#include "doctor.h"
#include "appointment.h"
#include "medical_records.h"
#include "prescription.h"
#include "pharmacy.h"
#include "billing.h"
#include "emergency.h"
#include "reports.h"

// Extract session token from cookie, header or auth token
static void extract_token(char *out_token, size_t max_len) {
    out_token[0] = '\0';

    // 1. Check HTTP_X_SESSION_TOKEN header
    const char *hdr_token = getenv("HTTP_X_SESSION_TOKEN");
    if (hdr_token && strlen(hdr_token) > 0) {
        strncpy(out_token, hdr_token, max_len - 1);
        out_token[max_len - 1] = '\0';
        return;
    }

    // 2. Check HTTP_AUTHORIZATION (Bearer token)
    const char *auth_hdr = getenv("HTTP_AUTHORIZATION");
    if (auth_hdr && strncmp(auth_hdr, "Bearer ", 7) == 0) {
        strncpy(out_token, auth_hdr + 7, max_len - 1);
        out_token[max_len - 1] = '\0';
        return;
    }

    // 3. Check HTTP_COOKIE (session_id=...)
    const char *cookie = getenv("HTTP_COOKIE");
    if (cookie) {
        const char *match = strstr(cookie, "session_id=");
        if (match) {
            const char *val = match + 11;
            const char *semi = strchr(val, ';');
            size_t len = semi ? (size_t)(semi - val) : strlen(val);
            if (len >= max_len) len = max_len - 1;
            strncpy(out_token, val, len);
            out_token[len] = '\0';
            return;
        }
    }
}

// Read JSON payload from stdin
static cJSON* read_json_body(void) {
    const char *cl_str = getenv("CONTENT_LENGTH");
    if (!cl_str) return NULL;
    int content_length = atoi(cl_str);
    if (content_length <= 0 || content_length > MAX_BUFFER_SIZE) return NULL;

    char *buf = (char *)malloc(content_length + 1);
    if (!buf) return NULL;

    size_t bytes_read = fread(buf, 1, content_length, stdin);
    buf[bytes_read] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json;
}

// Normalize path by stripping /api and trailing query or slash
static void normalize_path(char *dest, const char *raw_path, size_t max_len) {
    dest[0] = '\0';
    if (!raw_path) return;

    const char *p = raw_path;
    // Strip query string if present in PATH_INFO
    const char *q = strchr(p, '?');
    size_t len = q ? (size_t)(q - p) : strlen(p);
    if (len >= max_len) len = max_len - 1;
    strncpy(dest, p, len);
    dest[len] = '\0';

    // If starts with /api/, remove /api
    if (strncmp(dest, "/api/", 5) == 0) {
        memmove(dest, dest + 4, strlen(dest + 4) + 1);
    } else if (strcmp(dest, "/api") == 0) {
        strcpy(dest, "/");
    }

    // Strip trailing slash if not root
    size_t cur_len = strlen(dest);
    if (cur_len > 1 && dest[cur_len - 1] == '/') {
        dest[cur_len - 1] = '\0';
    }
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    // 1. Check for command-line diagnostics
    if (argc > 1 && strcmp(argv[1], "--test") == 0) {
        printf("Hospital Management System C Backend (C11, SQLite, cJSON)\n");
        printf("Version: %s\n", APP_NAME);
        printf("Diagnostics: All modules compiled and linked successfully.\n");
        return 0;
    }

    const char *request_method = getenv("REQUEST_METHOD");
    if (!request_method) {
        printf("Content-Type: text/plain\r\n\r\n");
        printf("Hospital Management System CGI Backend is ready.\nRun via HTTP server or test with '--test'.\n");
        return 0;
    }

    // Handle OPTIONS CORS preflight
    if (strcmp(request_method, "OPTIONS") == 0) {
        printf("Status: 204 No Content\r\n");
        response_send_cors_headers();
        printf("\r\n");
        return 0;
    }

    // Connect to SQLite Database
    sqlite3 *db = NULL;
    if (db_open(&db) != SQLITE_OK) {
        response_send_error(500, "Database connection failed");
        return 1;
    }

    // Clean up expired sessions periodically
    session_cleanup_expired(db);

    // Identify user via session token
    char session_token[64] = "";
    extract_token(session_token, sizeof(session_token));

    AuthUser current_user;
    memset(&current_user, 0, sizeof(current_user));
    int is_authenticated = 0;
    if (strlen(session_token) > 0) {
        is_authenticated = session_validate(db, session_token, &current_user);
    }

    // Extract path and query string
    const char *path_info = getenv("PATH_INFO");
    if (!path_info) path_info = getenv("REQUEST_URI");
    if (!path_info) path_info = "/";

    char path[256];
    normalize_path(path, path_info, sizeof(path));

    const char *query_string = getenv("QUERY_STRING");

    // Read body if POST or PUT
    cJSON *body = NULL;
    if (strcmp(request_method, "POST") == 0 || strcmp(request_method, "PUT") == 0) {
        body = read_json_body();
    }

    // ----------------- ROUTING TABLE -----------------

    // Authentication Endpoints
    if (strcmp(path, "/auth/login") == 0 && strcmp(request_method, "POST") == 0) {
        handle_auth_login(db, body);
    } else if (strcmp(path, "/auth/register") == 0 && strcmp(request_method, "POST") == 0) {
        handle_auth_register(db, body);
    } else if (strcmp(path, "/auth/logout") == 0 && strcmp(request_method, "POST") == 0) {
        handle_auth_logout(db, session_token);
    } else if (strcmp(path, "/auth/me") == 0 && strcmp(request_method, "GET") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Not authenticated");
        } else {
            handle_auth_me(db, &current_user);
        }
    }
    // Patients Endpoints
    else if (strcmp(path, "/patients") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else if (strcmp(request_method, "GET") == 0) {
            handle_patients_get(db, &current_user, query_string);
        } else if (strcmp(request_method, "POST") == 0) {
            handle_patients_post(db, &current_user, body);
        } else if (strcmp(request_method, "PUT") == 0) {
            handle_patients_put(db, &current_user, body);
        } else if (strcmp(request_method, "DELETE") == 0) {
            handle_patients_delete(db, &current_user, query_string);
        } else {
            response_send_error(405, "Method not allowed");
        }
    }
    // Doctors Endpoints
    else if (strcmp(path, "/doctors") == 0) {
        if (strcmp(request_method, "GET") == 0) {
            handle_doctors_get(db, is_authenticated ? &current_user : NULL, query_string);
        } else if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else if (strcmp(request_method, "POST") == 0) {
            handle_doctors_post(db, &current_user, body);
        } else if (strcmp(request_method, "PUT") == 0) {
            handle_doctors_put(db, &current_user, body);
        } else {
            response_send_error(405, "Method not allowed");
        }
    }
    // Departments Endpoints
    else if (strcmp(path, "/departments") == 0) {
        if (strcmp(request_method, "GET") == 0) {
            handle_departments_get(db);
        } else if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else if (strcmp(request_method, "POST") == 0) {
            handle_departments_post(db, &current_user, body);
        } else {
            response_send_error(405, "Method not allowed");
        }
    }
    // Appointments Endpoints
    else if (strcmp(path, "/appointments") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else if (strcmp(request_method, "GET") == 0) {
            handle_appointments_get(db, &current_user, query_string);
        } else if (strcmp(request_method, "POST") == 0) {
            handle_appointments_post(db, &current_user, body);
        } else if (strcmp(request_method, "PUT") == 0) {
            handle_appointments_put(db, &current_user, body);
        } else {
            response_send_error(405, "Method not allowed");
        }
    } else if (strcmp(path, "/appointments/cancel") == 0 && strcmp(request_method, "POST") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else {
            handle_appointments_cancel(db, &current_user, body);
        }
    }
    // Medical Records Endpoints
    else if (strcmp(path, "/medical-records") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else if (strcmp(request_method, "GET") == 0) {
            handle_medical_records_get(db, &current_user, query_string);
        } else if (strcmp(request_method, "POST") == 0) {
            handle_medical_records_post(db, &current_user, body);
        } else {
            response_send_error(405, "Method not allowed");
        }
    }
    // Prescriptions Endpoints
    else if (strcmp(path, "/prescriptions") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else if (strcmp(request_method, "GET") == 0) {
            handle_prescriptions_get(db, &current_user, query_string);
        } else if (strcmp(request_method, "POST") == 0) {
            handle_prescriptions_post(db, &current_user, body);
        } else {
            response_send_error(405, "Method not allowed");
        }
    }
    // Pharmacy / Medicines Endpoints
    else if (strcmp(path, "/medicines") == 0) {
        if (strcmp(request_method, "GET") == 0) {
            handle_medicines_get(db, is_authenticated ? &current_user : NULL, query_string);
        } else if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else if (strcmp(request_method, "POST") == 0) {
            handle_medicines_post(db, &current_user, body);
        } else if (strcmp(request_method, "PUT") == 0) {
            handle_medicines_put(db, &current_user, body);
        } else {
            response_send_error(405, "Method not allowed");
        }
    }
    // Billing / Invoices Endpoints
    else if (strcmp(path, "/invoices") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else if (strcmp(request_method, "GET") == 0) {
            handle_invoices_get(db, &current_user, query_string);
        } else if (strcmp(request_method, "POST") == 0) {
            handle_invoices_post(db, &current_user, body);
        } else {
            response_send_error(405, "Method not allowed");
        }
    } else if (strcmp(path, "/payments") == 0 && strcmp(request_method, "POST") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else {
            handle_payments_post(db, &current_user, body);
        }
    }
    // Emergency Queue Endpoints
    else if (strcmp(path, "/emergency") == 0) {
        if (strcmp(request_method, "GET") == 0) {
            handle_emergency_get(db, is_authenticated ? &current_user : NULL, query_string);
        } else if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else if (strcmp(request_method, "POST") == 0) {
            handle_emergency_post(db, &current_user, body);
        } else if (strcmp(request_method, "PUT") == 0) {
            handle_emergency_put(db, &current_user, body);
        } else {
            response_send_error(405, "Method not allowed");
        }
    }
    // Reports & Analytics Endpoints
    else if (strcmp(path, "/reports/dashboard") == 0 && strcmp(request_method, "GET") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else {
            handle_reports_dashboard(db, &current_user);
        }
    } else if (strcmp(path, "/reports/revenue") == 0 && strcmp(request_method, "GET") == 0) {
        if (!is_authenticated) {
            response_send_error(401, "Authentication required");
        } else {
            handle_reports_revenue(db, &current_user);
        }
    }
    // Route not found
    else {
        response_send_error(404, "Endpoint not found");
    }

    if (body) {
        cJSON_Delete(body);
    }
    db_close(db);
    return 0;
}
