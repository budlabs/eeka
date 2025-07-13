#pragma once

#include <linux/limits.h>

typedef enum {
    XDG_RUNTIME_DIR,
    XDG_CONFIG_HOME,
    XDG_CACHE_HOME,
    XDG_DATA_HOME
} xdg_directory_type_t;

char* xdg_get_directory(xdg_directory_type_t directory_type);
char* xdg_get_user_config_path(const char *program_name);
