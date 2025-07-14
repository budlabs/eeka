#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include "parser.h"
#include "config.h"
#include "eeka.h"

static KeyBinding bindings[MAX_BINDINGS];
static WindowRule window_rules[MAX_WINDOW_RULES];
static int binding_count = 0;
static int window_rule_count = 0;

DeviceConfig device_config = {0};

int parse_window_rule(FILE* config, const char* first_line);
int button_name_to_number(const char* button_name);
void parse_window_blacklist_line(WindowRule* rule, const char* blacklist_str);
void parse_device_blacklist_line(const char* blacklist_str);

const char* get_action_name(const Action* action) {
    static char action_str[64];
    action_str[0] = '\0';
    if (action->modifiers & MOD_CTRL)
        strcat(action_str, "Ctrl+");
    if (action->modifiers & MOD_SHIFT)
        strcat(action_str, "Shift+");
    if (action->modifiers & MOD_ALT)
        strcat(action_str, "Alt+");
    if (action->modifiers & MOD_SUPER)
        strcat(action_str, "Super+");
    if (action->key == XK_Page_Up) {
        strcat(action_str, "PageUp");
    } else if (action->key == XK_Page_Down) {
        strcat(action_str, "PageDown");
    } else if (action->key == XK_Return) {
        strcat(action_str, "Enter");
    } else if (action->key == XK_BackSpace) {
        strcat(action_str, "Backspace");
    } else if (action->key == XK_Delete) {
        strcat(action_str, "Delete");
    } else if (action->key == XK_Escape) {
        strcat(action_str, "Escape");
    } else if (action->key == XK_Tab) {
        strcat(action_str, "Tab");
    } else if (action->key == XK_space) {
        strcat(action_str, "Space");
    } else if (action->key == XK_Left) {
        strcat(action_str, "ArrowLeft");
    } else if (action->key == XK_Right) {
        strcat(action_str, "ArrowRight");
    } else if (action->key == XK_Up) {
        strcat(action_str, "ArrowUp");
    } else if (action->key == XK_Down) {
        strcat(action_str, "ArrowDown");
    } else if (action->key >= XK_F1 && action->key <= XK_F12) {
        char fkey[4];
        snprintf(fkey, sizeof(fkey), "F%d", (int)(action->key - XK_F1 + 1));
        strcat(action_str, fkey);
    } else {
        if (action->key < 128 && isprint(action->key)) {
            char ch[2] = {(char)action->key, '\0'};
            strcat(action_str, ch);
        } else {
            strcat(action_str, "?");
        }
    }
    return action_str;
}

int parse_action_string(const char* str, Action* action) {
    char mod_key[128];
    strncpy(mod_key, str, sizeof(mod_key) - 1);
    mod_key[sizeof(mod_key) - 1] = '\0';
    action->modifiers = 0;
    action->key = 0;
    char* token = strtok(mod_key, "+");
    char* last_token = NULL;
    while (token) {
        last_token = token;
        if (strcasecmp(token, "Ctrl") == 0) {
            action->modifiers |= MOD_CTRL;
        } else if (strcasecmp(token, "Shift") == 0) {
            action->modifiers |= MOD_SHIFT;
        } else if (strcasecmp(token, "Alt") == 0) {
            action->modifiers |= MOD_ALT;
        } else if (strcasecmp(token, "Super") == 0) {
            action->modifiers |= MOD_SUPER;
        } else {
            break;
        }
        token = strtok(NULL, "+");
    }
    if (!last_token) {
        msg(LOG_ERR, "No key specified in action: %s", str);
        return 0;
    }
    if (strcasecmp(last_token, "PageUp") == 0) {
        action->key = XK_Page_Up;
    } else if (strcasecmp(last_token, "PageDown") == 0) {
        action->key = XK_Page_Down;
    } else if (strcasecmp(last_token, "Enter") == 0) {
        action->key = XK_Return;
    } else if (strcasecmp(last_token, "Backspace") == 0) {
        action->key = XK_BackSpace;
    } else if (strcasecmp(last_token, "Delete") == 0) {
        action->key = XK_Delete;
    } else if (strcasecmp(last_token, "Escape") == 0) {
        action->key = XK_Escape;
    } else if (strcasecmp(last_token, "Tab") == 0) {
        action->key = XK_Tab;
    } else if (strcasecmp(last_token, "Space") == 0) {
        action->key = XK_space;
    } else if (strcasecmp(last_token, "ArrowLeft") == 0 || strcasecmp(last_token, "Left") == 0) {
        action->key = XK_Left;
    } else if (strcasecmp(last_token, "ArrowRight") == 0 || strcasecmp(last_token, "Right") == 0) {
        action->key = XK_Right;
    } else if (strcasecmp(last_token, "ArrowUp") == 0 || strcasecmp(last_token, "Up") == 0) {
        action->key = XK_Up;
    } else if (strcasecmp(last_token, "ArrowDown") == 0 || strcasecmp(last_token, "Down") == 0) {
        action->key = XK_Down;
    } else if (strncasecmp(last_token, "F", 1) == 0 && strlen(last_token) <= 3) {
        int fkey = atoi(last_token + 1);
        if (fkey >= 1 && fkey <= 99) {
            action->key = XK_F1 + (fkey - 1);
        } else {
            msg(LOG_ERR, "Invalid function key: %s", last_token);
            return 0;
        }
    } else if (strlen(last_token) == 1) {
        action->key = toupper(last_token[0]);
    } else {
        msg(LOG_ERR, "Unknown key: %s", last_token);
        return 0;
    }
    return 1;
}

const char* get_button_name(int button) {
    switch (button) {
        case 1: return "LButton (1)";
        case 2: return "MButton (2)";
        case 3: return "RButton (3)";
        case 4: return "ScrollUp (4)";
        case 5: return "ScrollDown (5)";
        case 8: return "BButton (8)";
        case 9: return "FButton (9)";
        default: {
            static char buff[16];
            snprintf(buff, sizeof(buff), "Button%d", button);
            return buff;
        }
    }
}

int button_name_to_number(const char* button_name) {
    int button_num = 0;
    if (sscanf(button_name, "Button%d", &button_num) == 1) {
        return button_num;
    }
    if (strcasecmp(button_name, "LButton") == 0) return 1;
    if (strcasecmp(button_name, "MButton") == 0) return 2;
    if (strcasecmp(button_name, "RButton") == 0) return 3;
    if (strcasecmp(button_name, "BButton") == 0) return 8;
    if (strcasecmp(button_name, "FButton") == 0) return 9;
    if (strcasecmp(button_name, "ScrollUp") == 0) return 4;
    if (strcasecmp(button_name, "ScrollDown") == 0) return 5;
    return 0;
}

const char* config_path_get_default(void) {
    static char config_path[PATH_MAX];
    const char* xdg_config_home;
    const char* home;
    FILE* test_file;
    xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home && *xdg_config_home) {
        snprintf(config_path, PATH_MAX, "%s/eeka/config", xdg_config_home);
        test_file = fopen(config_path, "r");
        if (test_file) {
            fclose(test_file);
            msg(LOG_DEBUG, "Found config at $XDG_CONFIG_HOME: %s", config_path);
            return config_path;
        }
    }
    home = getenv("HOME");
    if (home && *home) {
        snprintf(config_path, PATH_MAX, "%s/.config/eeka/config", home);
        test_file = fopen(config_path, "r");
        if (test_file) {
            fclose(test_file);
            msg(LOG_DEBUG, "Found config at $HOME/.config: %s", config_path);
            return config_path;
        }
    }
    const char* xdg_data_dirs = getenv("XDG_DATA_DIRS");
    if (!xdg_data_dirs || !*xdg_data_dirs) {
        xdg_data_dirs = "/usr/local/share:/usr/share";
    }
    char data_dirs[PATH_MAX];
    strncpy(data_dirs, xdg_data_dirs, PATH_MAX - 1);
    data_dirs[PATH_MAX - 1] = '\0';
    char* dir = strtok(data_dirs, ":");
    while (dir) {
        snprintf(config_path, PATH_MAX, "%s/eeka/config", dir);
        test_file = fopen(config_path, "r");
        if (test_file) {
            fclose(test_file);
            msg(LOG_DEBUG, "Found config at XDG_DATA_DIR: %s", config_path);
            return config_path;
        }
        dir = strtok(NULL, ":");
    }
    snprintf(config_path, PATH_MAX, "/etc/eeka/config");
    test_file = fopen(config_path, "r");
    if (test_file) {
        fclose(test_file);
        msg(LOG_DEBUG, "Found config at system location: %s", config_path);
        return config_path;
    }
    if (xdg_config_home && *xdg_config_home) {
        snprintf(config_path, PATH_MAX, "%s/eeka/config", xdg_config_home);
    } else if (home && *home) {
        snprintf(config_path, PATH_MAX, "%s/.config/eeka/config", home);
    } else {
        snprintf(config_path, PATH_MAX, "./eeka.config");
    }
    msg(LOG_DEBUG, "No existing config found, will use: %s", config_path);
    return config_path;
}

int parse_binding_line(const char* line, KeyBinding* binding) {

    char button1_name[32] = {0};
    char button2_name[32] = {0};

    char action[64] = {0};

    if (sscanf(line, "%31s & %31s = %63s", button1_name, button2_name, action) == 3) {
        int button1 = button_name_to_number(button1_name);
        int button2 = button_name_to_number(button2_name);
        if (button1 == 0 || button2 == 0) {
            msg(LOG_ERR, "Invalid button name in: %s", line);
            return 0;
        }
        binding->button1 = button1;
        binding->button2 = button2;
    }
    else if (sscanf(line, "%31s = %63s", button1_name, action) == 2) {
        int button1 = button_name_to_number(button1_name);
        if (button1 == 0) {
            msg(LOG_ERR, "Invalid button name in: %s", line);
            return 0;
        }
        binding->button1 = button1;
        binding->button2 = 0;
    }
    else {
        msg(LOG_ERR, "Invalid binding format: %s", line);
        return 0;
    }

    if (!parse_action_string(action, &binding->action)) {
        msg(LOG_ERR, "Invalid action: %s", action);
        return 0;
    }

    return 1;
}

void parse_window_blacklist_line(WindowRule* rule, const char* blacklist_str) {
    char* token_start = (char*)blacklist_str;
    
    while (*token_start && rule->blacklist_count < MAX_BUTTONS_PER_RULE) {
        // Skip whitespace and commas
        while (*token_start && (isspace(*token_start) || *token_start == ',')) {
            token_start++;
        }
        
        if (!*token_start) break;
        
        // Find end of token
        char* token_end = token_start;
        while (*token_end && *token_end != ',' && !isspace(*token_end)) {
            token_end++;
        }
        
        // Extract token
        char button_name[32] = {0};
        int len = token_end - token_start;
        if (len >= (int)sizeof(button_name)) len = (int)sizeof(button_name) - 1;
        strncpy(button_name, token_start, len);
        button_name[len] = '\0';
        
        // Convert to button number
        int button_num = button_name_to_number(button_name);
        if (button_num > 0) {
            rule->blacklisted_buttons[rule->blacklist_count++] = button_num;
            msg(LOG_NOTICE, "Blacklisted button %s (%d) for window rule", 
                button_name, button_num);
        } else {
            msg(LOG_ERR, "Invalid button name in blacklist: %s", button_name);
        }
        
        token_start = token_end;
    }
}

int parse_window_rule(FILE* config, const char* first_line) {
    char instance[128] = {0};
    char class_name[128] = {0};
    char* criteria_start = strchr(first_line, '[');
    if (!criteria_start) {
        msg(LOG_ERR, "Missing criteria for window rule: %s", first_line);
        return 0;
    }
    char* criteria_end = strchr(criteria_start, ']');
    if (!criteria_end) {
        msg(LOG_ERR, "Missing closing bracket for window rule criteria: %s", first_line);
        return 0;
    }
    char criteria[128] = {0};
    int criteria_len = criteria_end - criteria_start - 1;
    if (criteria_len >= (int)sizeof(criteria)) criteria_len = (int)sizeof(criteria) - 1;
    strncpy(criteria, criteria_start + 1, criteria_len);
    criteria[criteria_len] = '\0';
    char* instance_part = strstr(criteria, "instance=");
    char* class_part = strstr(criteria, "class=");
    if (instance_part) {
        instance_part += 9;
        char* end = strchr(instance_part, ',');
        if (!end) end = instance_part + strlen(instance_part);
        int len = end - instance_part;
        if (len > (int)sizeof(instance) - 1) len = (int)sizeof(instance) - 1;
        strncpy(instance, instance_part, len);
        instance[len] = '\0';
        char* p = instance;
        while (*p && isspace(*p)) p++;
        memmove(instance, p, strlen(p) + 1);
        p = instance + strlen(instance) - 1;
        while (p >= instance && isspace(*p)) *p-- = '\0';
    }
    if (class_part) {
        class_part += 6;
        char* end = strchr(class_part, ',');
        if (!end) end = class_part + strlen(class_part);
        int len = end - class_part;
        if (len > (int)sizeof(class_name) - 1) len = (int)sizeof(class_name) - 1;
        strncpy(class_name, class_part, len);
        class_name[len] = '\0';
        char* p = class_name;
        while (*p && isspace(*p)) p++;
        memmove(class_name, p, strlen(p) + 1);
        p = class_name + strlen(class_name) - 1;
        while (p >= class_name && isspace(*p)) *p-- = '\0';
    }
    if (instance[0] == '\0' && class_name[0] == '\0') {
        msg(LOG_ERR, "Window rule missing criteria: %s", first_line);
        return 0;
    }
    if (window_rule_count >= MAX_WINDOW_RULES) {
        msg(LOG_ERR, "Too many window rules (max %d)", MAX_WINDOW_RULES);
        return 0;
    }
    WindowRule* rule = &window_rules[window_rule_count++];
    snprintf(rule->instance, sizeof(rule->instance), "%s", instance);
    snprintf(rule->class_name, sizeof(rule->class_name), "%s", class_name);
    rule->binding_count = 0;
    rule->blacklist_count = 0;  // Initialize blacklist count
    msg(LOG_NOTICE, "Created window rule for instance='%s', class='%s'",
               rule->instance, rule->class_name);
    char line[256];
    char trimmed_line[256];
    while (fgets(line, sizeof(line), config)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        char* p = line;
        while (*p && isspace(*p)) p++;
        strncpy(trimmed_line, p, sizeof(trimmed_line)-1);
        trimmed_line[sizeof(trimmed_line)-1] = '\0';
        if (trimmed_line[0] == '\0')
            continue;
        if (trimmed_line[0] == '}') {
            return 1;
        }
        // Add blacklist parsing within window rule
        if (strncmp(trimmed_line, "blacklist = ", 12) == 0) {
            char* blacklist_str = trimmed_line + 12;
            parse_window_blacklist_line(rule, blacklist_str);
            continue;
        }
        KeyBinding binding = {0};
        if (parse_binding_line(trimmed_line, &binding)) {
            if (rule->binding_count < MAX_BINDINGS_PER_RULE) {
                rule->bindings[rule->binding_count++] = binding;
                msg(LOG_NOTICE, "Added window rule binding: %s%s%s = %s",
                          get_button_name(binding.button1),
                          binding.button2 ? " & " : "",
                          binding.button2 ? get_button_name(binding.button2) : "",
                          get_action_name(&binding.action));
            } else {
                msg(LOG_ERR, "Too many bindings for window rule (max %d)", MAX_BINDINGS_PER_RULE);
            }
        }
    }
    msg(LOG_ERR, "Missing closing brace for window rule");
    return 0;
}

int parse_config_file(const char* filename) {

    char real_path[PATH_MAX];

    if (!filename || strlen(filename) == 0) {
        snprintf(real_path, sizeof(real_path), "%s", config_path_get_default());
        filename = real_path;
    }

    if (filename[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) {
            msg(LOG_ERR, "Could not determine home directory");
            return 0;
        }
        snprintf(real_path, PATH_MAX, "%s%s", home, filename + 1);
    } else {
        strncpy(real_path, filename, PATH_MAX);
        real_path[PATH_MAX-1] = '\0';
    }

    FILE* config = fopen(real_path, "r");
    if (!config) {
        msg(LOG_ERR, "Could not open config file %s: %s", real_path, strerror(errno));
        return 0;
    }

    msg(LOG_NOTICE, "Reading configuration from %s", real_path);

    char line[1024];
    char trimmed_line[1024];
    char merged_line[1024] = {0};
    binding_count = 0;
    window_rule_count = 0;
    device_config.device_blacklist_count = 0;
    int in_continuation = 0;

    while (fgets(line, sizeof(line), config) && binding_count < MAX_BINDINGS) {
        
        if (!in_continuation && (line[0] == '#' || line[0] == '\n' || line[0] == '\r'))
            continue;

        char* newline = strchr(line, '\n');

        if (newline) *newline = '\0';

        int line_len = strlen(line);
        in_continuation = 0;

        if (line_len > 0 && line[line_len - 1] == '\\') {
            line[line_len - 1] = '\0';
            line_len--;
            in_continuation = 1;
            if (merged_line[0] != '\0') {
                strncat(merged_line, " ", sizeof(merged_line) - strlen(merged_line) - 1);
            }
            strncat(merged_line, line, sizeof(merged_line) - strlen(merged_line) - 1);
            continue;
        }

        if (merged_line[0] != '\0') {
            strncat(merged_line, " ", sizeof(merged_line) - strlen(merged_line) - 1);
            strncat(merged_line, line, sizeof(merged_line) - strlen(merged_line) - 1);
            strncpy(line, merged_line, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
            merged_line[0] = '\0';
        }

        char* p = line;
        while (*p && isspace(*p)) p++;

        strncpy(trimmed_line, p, sizeof(trimmed_line)-1);
        trimmed_line[sizeof(trimmed_line)-1] = '\0';

        if (trimmed_line[0] == '\0')
            continue;

        if (strncmp(trimmed_line, "window ", 7) == 0) {
            parse_window_rule(config, trimmed_line);
            continue;
        }

        if (strncmp(trimmed_line, "device_blacklist = ", 19) == 0) {
            parse_device_blacklist_line(trimmed_line + 19);
            continue;
        }

        KeyBinding binding = {0};

        if (parse_binding_line(trimmed_line, &binding)) {
            if (binding_count < MAX_BINDINGS) {
                bindings[binding_count++] = binding;
                msg(LOG_NOTICE, "Added binding: %s%s%s = %s",
                          get_button_name(binding.button1),
                          binding.button2 ? " & " : "",
                          binding.button2 ? get_button_name(binding.button2) : "",
                          get_action_name(&binding.action));
            } else {
                msg(LOG_ERR, "Too many bindings (max %d)", MAX_BINDINGS);
            }
        }
    }

    fclose(config);

    if (binding_count == 0) {
        msg(LOG_WARNING, "No valid bindings defined in config file %s", real_path);
    }

    return binding_count;
}

void parse_device_blacklist_line(const char* blacklist_str) {
    char* token_start = (char*)blacklist_str;
    
    while (*token_start && device_config.device_blacklist_count < MAX_DEVICE_BLACKLIST) {
        while (*token_start && (isspace(*token_start) || *token_start == ',')) {
            token_start++;
        }
        
        if (!*token_start) break;
        
        char* token_end = token_start;
        while (*token_end && *token_end != ',' && !isspace(*token_end)) {
            token_end++;
        }
        
        char device_name[MAX_DEVICE_NAME_LENGTH] = {0};
        int len = token_end - token_start;
        if (len >= MAX_DEVICE_NAME_LENGTH) len = MAX_DEVICE_NAME_LENGTH - 1;
        strncpy(device_name, token_start, len);
        device_name[len] = '\0';
        
        strncpy(device_config.blacklisted_devices[device_config.device_blacklist_count], 
                device_name, MAX_DEVICE_NAME_LENGTH - 1);
        device_config.blacklisted_devices[device_config.device_blacklist_count][MAX_DEVICE_NAME_LENGTH - 1] = '\0';
        device_config.device_blacklist_count++;
        
        msg(LOG_NOTICE, "Added device to blacklist: %s", device_name);
        
        token_start = token_end;
    }
}

int is_device_blacklisted(const char* device_name) {
   for (int i = 0; i < device_config.device_blacklist_count; i++) {
       if (strstr(device_name, device_config.blacklisted_devices[i])) {
           return 1;
       }
   }
   return 0;
}

int is_button_blacklisted(const char* instance, const char* class_name, int button) {
    for (int i = 0; i < window_rule_count; i++) {
        WindowRule* rule = &window_rules[i];
        int match = 1;
        
        if (rule->instance[0] && strcmp(rule->instance, instance) != 0) {
            match = 0;
        }
        if (rule->class_name[0] && strcmp(rule->class_name, class_name) != 0) {
            match = 0;
        }
        
        if (match) {
            for (int j = 0; j < rule->blacklist_count; j++) {
                if (rule->blacklisted_buttons[j] == button) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

const Action* get_action_for_buttons(int first_button, int second_button) {
    for (int i = 0; i < binding_count; i++) {
        if (second_button > 0) {
            if (bindings[i].button1 == first_button && bindings[i].button2 == second_button) {
                return &bindings[i].action;
            }
        }
        else if (bindings[i].button1 == first_button && bindings[i].button2 == 0) {
            return &bindings[i].action;
        }
    }
    return NULL;
}

const Action* get_action_for_window(const char* instance, const char* class_name,
                                   int first_button, int second_button) {
    for (int i = 0; i < window_rule_count; i++) {
        WindowRule* rule = &window_rules[i];
        int match = 1;
        if (rule->instance[0] && strcmp(rule->instance, instance) != 0) {
            match = 0;
        }
        if (rule->class_name[0] && strcmp(rule->class_name, class_name) != 0) {
            match = 0;
        }
        if (match) {
            for (int j = 0; j < rule->binding_count; j++) {
                if (second_button > 0) {
                    if (rule->bindings[j].button1 == first_button &&
                        rule->bindings[j].button2 == second_button) {
                        return &rule->bindings[j].action;
                    }
                }
                else if (rule->bindings[j].button1 == first_button &&
                         rule->bindings[j].button2 == 0) {
                    return &rule->bindings[j].action;
                }
            }
        }
    }
    return get_action_for_buttons(first_button, second_button);
}
