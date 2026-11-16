#ifndef CONFIG_H
#define CONFIG_H

#define APP_NAME "Hospital Management System"
#define APP_VERSION "1.0.0"
#define DEFAULT_DB_PATH "database/hospital.db"
#define SESSION_TTL_SECONDS 86400  // 24 hours
#define MAX_BUFFER_SIZE 65536
#define MAX_TOKEN_LEN 64
#define MAX_ROLE_LEN 32
#define MAX_USERNAME_LEN 64

typedef struct {
    char db_path[256];
    int session_timeout;
    int port;
} AppConfig;

void config_init(AppConfig *config);
const char* config_get_db_path(void);

#endif // CONFIG_H

