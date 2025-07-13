#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "xdg.h"
#include <libgen.h>

char* xdg_get_directory(xdg_directory_type_t directory_type) {
    const char *environment_variable = NULL;
    char *fallback_path = NULL;
    
    switch (directory_type) {
        case XDG_RUNTIME_DIR: {
            environment_variable = "XDG_RUNTIME_DIR";
            size_t fallback_size = snprintf(NULL, 0, "/tmp/halen-%d", getuid()) + 1;
            fallback_path = malloc(fallback_size);
            if (fallback_path) {
                snprintf(fallback_path, fallback_size, "/tmp/halen-%d", getuid());
            }
            break;
        }
        case XDG_CONFIG_HOME: {
            environment_variable = "XDG_CONFIG_HOME";
            const char *home = getenv("HOME");
            if (home) {
                size_t fallback_size = strlen(home) + strlen("/.config") + 1;
                fallback_path = malloc(fallback_size);
                if (fallback_path) {
                    snprintf(fallback_path, fallback_size, "%s/.config", home);
                }
            }
            break;
        }
        case XDG_CACHE_HOME: {
            environment_variable = "XDG_CACHE_HOME";
            const char *home = getenv("HOME");
            if (home) {
                size_t fallback_size = strlen(home) + strlen("/.cache") + 1;
                fallback_path = malloc(fallback_size);
                if (fallback_path) {
                    snprintf(fallback_path, fallback_size, "%s/.cache", home);
                }
            }
            break;
        }
        case XDG_DATA_HOME: {
            environment_variable = "XDG_DATA_HOME";
            const char *home = getenv("HOME");
            if (home) {
                size_t fallback_size = strlen(home) + strlen("/.local/share") + 1;
                fallback_path = malloc(fallback_size);
                if (fallback_path) {
                    snprintf(fallback_path, fallback_size, "%s/.local/share", home);
                }
            }
            break;
        }
        default:
            errno = EINVAL;
            return NULL;
    }
    
    if (!fallback_path) {
        errno = ENOMEM;
        return NULL;
    }
    
    char *xdg_value = getenv(environment_variable);
    if (xdg_value && strlen(xdg_value) > 0) {
        free(fallback_path);
        return strdup(xdg_value);
    }
    
    if (mkdir(fallback_path, 0700) == -1 && errno != EEXIST) {
        if (directory_type == XDG_RUNTIME_DIR) {
            free(fallback_path);
            return strdup("/tmp");
        }
    }
    
    return fallback_path;
}

static int copy_file(const char *source, const char *destination) {
    FILE *source_file = fopen(source, "r");
    if (!source_file) {
        return 0;
    }
    
    char *destination_directory = strdup(destination);
    char *directory_path = dirname(destination_directory);
    
    if (mkdir(directory_path, 0755) == -1 && errno != EEXIST) {
        free(destination_directory);
        fclose(source_file);
        return 0;
    }
    free(destination_directory);
    
    FILE *destination_file = fopen(destination, "w");
    if (!destination_file) {
        fclose(source_file);
        return 0;
    }
    
    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, destination_file) != bytes_read) {
            fclose(source_file);
            fclose(destination_file);
            return 0;
        }
    }
    
    fclose(source_file);
    fclose(destination_file);
    return 1;
}

static char* find_system_config_file(const char *program_name) {
    char *data_directories = getenv("XDG_DATA_DIRS");
    if (!data_directories || strlen(data_directories) == 0) {
        data_directories = "/usr/local/share:/usr/share";
    }
    
    char *directories_copy = strdup(data_directories);
    char *current_directory = strtok(directories_copy, ":");
    
    while (current_directory) {
        size_t path_length = strlen(current_directory) + strlen(program_name) + strlen("/config") + 2;
        char *config_path = malloc(path_length);
        if (config_path) {
            snprintf(config_path, path_length, "%s/%s/config", current_directory, program_name);
            if (access(config_path, R_OK) == 0) {
                free(directories_copy);
                return config_path;
            }
            free(config_path);
        }
        current_directory = strtok(NULL, ":");
    }
    
    free(directories_copy);
    
    size_t fallback_length = strlen("/etc/") + strlen(program_name) + strlen("/config") + 1;
    char *fallback_path = malloc(fallback_length);
    if (fallback_path) {
        snprintf(fallback_path, fallback_length, "/etc/%s/config", program_name);
        if (access(fallback_path, R_OK) == 0) {
            return fallback_path;
        }
        free(fallback_path);
    }
    
    errno = ENOENT;
    return NULL;
}

char* xdg_get_user_config_path(const char *program_name) {
    char *config_home_directory = xdg_get_directory(XDG_CONFIG_HOME);
    if (!config_home_directory) {
        return NULL;
    }
    
    size_t user_config_length = strlen(config_home_directory) + strlen(program_name) + strlen("/config") + 2;
    char *user_config_path = malloc(user_config_length);
    if (!user_config_path) {
        free(config_home_directory);
        errno = ENOMEM;
        return NULL;
    }
    
    snprintf(user_config_path, user_config_length, "%s/%s/config", config_home_directory, program_name);
    
    if (access(user_config_path, R_OK) == 0) {
        free(config_home_directory);
        return user_config_path;
    }
    
    char *data_home_directory = xdg_get_directory(XDG_DATA_HOME);
    if (data_home_directory) {
        size_t data_config_length = strlen(data_home_directory) + strlen(program_name) + strlen("/config") + 2;
        char *data_config_path = malloc(data_config_length);
        if (data_config_path) {
            snprintf(data_config_path, data_config_length, "%s/%s/config", data_home_directory, program_name);
            if (access(data_config_path, R_OK) == 0) {
                free(config_home_directory);
                free(data_home_directory);
                free(user_config_path);
                return data_config_path;
            }
            free(data_config_path);
        }
        free(data_home_directory);
    }
    
    char *system_config_path = find_system_config_file(program_name);
    if (!system_config_path) {
        free(config_home_directory);
        free(user_config_path);
        return NULL;
    }
    
    if (!copy_file(system_config_path, user_config_path)) {
        free(config_home_directory);
        free(user_config_path);
        free(system_config_path);
        return NULL;
    }
    
    free(config_home_directory);
    free(system_config_path);
    return user_config_path;
}
