#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

static AppConfig global_config;

void config_init(AppConfig *config) {
    const char *env_db = getenv("HOSPITAL_DB_PATH");
    if (env_db != NULL && strlen(env_db) > 0) {
        snprintf(config->db_path, sizeof(config->db_path), "%s", env_db);
    } else {
        // Probe common relative paths
        const char *candidates[] = {
            "database/hospital.db",
            "../database/hospital.db",
            "../../database/hospital.db",
            "hospital-management-system/database/hospital.db",
            "C:/Users/Sugumar/Music/New folder (2)/hospital-management-system/database/hospital.db"
        };
        int found = 0;
        for (int i = 0; i < 5; i++) {
            if (access(candidates[i], F_OK) == 0) {
                snprintf(config->db_path, sizeof(config->db_path), "%s", candidates[i]);
                found = 1;
                break;
            }
        }
        if (!found) {
            snprintf(config->db_path, sizeof(config->db_path), "%s", DEFAULT_DB_PATH);
        }
    }
    config->session_timeout = SESSION_TTL_SECONDS;
    config->port = 8080;
    global_config = *config;
}

const char* config_get_db_path(void) {
    if (strlen(global_config.db_path) == 0) {
        config_init(&global_config);
    }
    return global_config.db_path;
}

