#pragma once

#include <xcb/xcb.h>
#include <stdio.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_BINDINGS 10
#define MAX_BUTTONS_PER_RULE 3
#define MAX_WINDOW_RULES 20
#define MAX_BINDINGS_PER_RULE 20

#define MAX_DEVICE_BLACKLIST 10
#define MAX_DEVICE_NAME_LENGTH 64

typedef struct {
    char blacklisted_devices[MAX_DEVICE_BLACKLIST][MAX_DEVICE_NAME_LENGTH];
    int device_blacklist_count;
} DeviceConfig;

extern DeviceConfig device_config;

typedef struct {
    unsigned int modifiers;
    unsigned int key;
} Action;

typedef struct {
    int button1;
    int button2;
    Action action;
} KeyBinding;

typedef struct {
    char instance[128];
    char class_name[128];
    KeyBinding bindings[MAX_BINDINGS_PER_RULE];
    int binding_count;
    int blacklisted_buttons[MAX_BUTTONS_PER_RULE];
    int blacklist_count;
} WindowRule;

int           parse_config_file(const char* filename);
const char*   get_action_name(const Action* combo);
const char*   get_button_name(int button);
const Action* get_action_for_buttons(int first_button, int second_button);
const Action* get_action_for_window(const char* instance, const char* class_name, int first_button, int second_button);
int           is_button_blacklisted(const char* instance, const char* class_name, int button);
int           is_device_blacklisted(const char* device_name);
