#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xtest.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <getopt.h>
#include <linux/limits.h>
#include <sys/select.h>
#include <errno.h>

#include "config.h"
#include "eeka.h"

static EvdevContext evdev_ctx = {0};

xcb_connection_t *connection = NULL;
xcb_screen_t *screen = NULL;

char pidfile_path[PATH_MAX];

int running = 1;
int grabbing_enabled = 1;
int enabled = 1;
int verbose = 0;

ButtonState button_state = {0};

void handle_signal(int sig);
void toggle_signal_handler(int sig);
xcb_window_t get_window_at_pointer(xcb_connection_t *conn);
xcb_window_t find_target_window(xcb_connection_t *conn);
WindowClassInfo get_window_class_info(xcb_connection_t *conn, xcb_window_t window);
void send_key_combination(const Action* action, xcb_window_t target_window);
int handle_key_binding(int first_button, int second_button);
void handle_button_press(int button);
void handle_button_release(int button);
void handle_scroll_event(int scroll_direction);
void simulate_button_click(int button, xcb_window_t target_window);

void handle_signal(int sig) {
    msg(LOG_NOTICE, "Received signal %d, shutting down", sig);
    running = 0;
}

void toggle_signal_handler(int sig) {
    (void)sig;
    enabled = !enabled;
    msg(LOG_NOTICE, "Toggled enabled state: %s", enabled ? "ON" : "OFF");
}

xcb_window_t get_window_at_pointer(xcb_connection_t *conn) {
    xcb_query_pointer_cookie_t cookie = xcb_query_pointer(conn, screen->root);
    xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(conn, cookie, NULL);
    if (!reply) {
        return XCB_NONE;
    }
    xcb_window_t win = reply->child;
    free(reply);
    return win;
}

WindowClassInfo get_window_class_info(xcb_connection_t *conn, xcb_window_t window) {
    
    WindowClassInfo info = {{0}, {0}, 0};

    xcb_get_property_cookie_t cookie = xcb_get_property(conn, 0, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 1024);
    xcb_get_property_reply_t *reply  = xcb_get_property_reply(conn, cookie, NULL);

    if (reply && reply->type == XCB_ATOM_STRING && reply->format == 8 && reply->length > 0) {
        
        char *data = (char *)xcb_get_property_value(reply);
        int len = xcb_get_property_value_length(reply);

        if (len > 0) {
            char *instance = data;
            char *class_name = NULL;
            for (int i = 0; i < len - 1; i++) {
                if (data[i] == '\0') {
                    class_name = &data[i + 1];
                    break;
                }
            }

            if (instance) 
                strncpy(info.instance, instance, sizeof(info.instance) - 1);
            if (class_name)
                strncpy(info.class_name, class_name, sizeof(info.class_name) - 1);

            info.valid = 1;
        }
    }

    if (reply) free(reply);
    return info;
}

xcb_window_t find_target_window(xcb_connection_t *conn) {

    xcb_window_t window = get_window_at_pointer(conn);

    if (window == XCB_NONE || window == screen->root) {
        return XCB_NONE;
    }

    xcb_query_tree_cookie_t tree_cookie = xcb_query_tree(conn, window);
    xcb_query_tree_reply_t *tree_reply = xcb_query_tree_reply(conn, tree_cookie, NULL);

    if (tree_reply && tree_reply->children_len > 0) {
        xcb_window_t *children = xcb_query_tree_children(tree_reply);
        window = children[0];
    }  else if (tree_reply && tree_reply->children_len == 0) {
        msg(LOG_DEBUG, "No child windows found for %u, using it as target", window);
    }

    WindowClassInfo info = get_window_class_info(conn, window);
    msg(LOG_DEBUG, "Target window found: %u (instance='%s', class='%s')",
                window, info.instance, info.class_name);
    
    if (tree_reply) free(tree_reply);
    return window;
}

#define SEND_KEY_PRESS(keycode, target) \
    xcb_test_fake_input(connection, XCB_KEY_PRESS, keycode, XCB_CURRENT_TIME, target, 0, 0, 0)

#define SEND_KEY_RELEASE(keycode, target) \
    xcb_test_fake_input(connection, XCB_KEY_RELEASE, keycode, XCB_CURRENT_TIME, target, 0, 0, 0)

int handle_key_binding(int first_button, int second_button) {
    xcb_window_t target_window = find_target_window(connection);
    WindowClassInfo info = {{0}, {0}, 0};
    const Action* action = NULL;

    if (target_window != XCB_NONE) {
        info = get_window_class_info(connection, target_window);
    }
    
    if (info.valid) {
        action = get_action_for_window(info.instance, info.class_name, first_button, second_button);
    } else {
        action = get_action_for_buttons(first_button, second_button);
    }

    if (action) {
        msg(LOG_DEBUG, "Found binding for button %d + %d: %s",
                    first_button, second_button, get_action_name(action));

        grabbing_enabled = 0;
        send_key_combination(action, target_window);
        grabbing_enabled = 1;
        button_state.combo_used = 1;
        return 1;
    } else {
        msg(LOG_DEBUG, "No binding found for buttons %d + %d",
                  first_button, second_button);
        return 0;
    }
}

void send_key_combination(const Action* action, xcb_window_t target_window) {
    msg(LOG_DEBUG, "Sending key combination: %s", get_action_name(action));
    
    if (target_window == XCB_NONE) {
        msg(LOG_DEBUG, "No valid target window found or window is blacklisted");
        return;
    }

    xcb_keycode_t key_code = 0;
    xcb_key_symbols_t* key_symbols = xcb_key_symbols_alloc(connection);

    if (!key_symbols) {
        msg(LOG_ERR, "Failed to allocate key symbols");
        return;
    }

    xcb_keycode_t* key_codes = xcb_key_symbols_get_keycode(key_symbols, action->key);

    if (key_codes) {
        key_code = key_codes[0];
        free(key_codes);
    } else {
        msg(LOG_ERR, "No keycode found for key: %u", (unsigned int)action->key);
        xcb_key_symbols_free(key_symbols);
        return;
    }

    xcb_key_symbols_free(key_symbols);
    
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, target_window, XCB_CURRENT_TIME);
    xcb_flush(connection);

    usleep(1000); // 1ms delay

    if (action->modifiers & MOD_CTRL)
        SEND_KEY_PRESS(XCB_KEY_CONTROL_L, target_window);
    if (action->modifiers & MOD_SHIFT)
        SEND_KEY_PRESS(XCB_KEY_SHIFT_L, target_window);
    if (action->modifiers & MOD_ALT)
        SEND_KEY_PRESS(XCB_KEY_ALT_L, target_window);
    if (action->modifiers & MOD_SUPER)
        SEND_KEY_PRESS(XCB_KEY_SUPER_L, target_window);

    SEND_KEY_PRESS(key_code, target_window);
    xcb_flush(connection);
    usleep(1000);
    SEND_KEY_RELEASE(key_code, target_window);

    if (action->modifiers & MOD_SUPER)
        SEND_KEY_RELEASE(XCB_KEY_SUPER_L, target_window);
    if (action->modifiers & MOD_ALT)
        SEND_KEY_RELEASE(XCB_KEY_ALT_L, target_window);
    if (action->modifiers & MOD_SHIFT)
        SEND_KEY_RELEASE(XCB_KEY_SHIFT_L, target_window);
    if (action->modifiers & MOD_CTRL)
        SEND_KEY_RELEASE(XCB_KEY_CONTROL_L, target_window);
        
    xcb_flush(connection);
}

int toggle_eeka_daemon(void) {
    FILE *pidfile;
    pid_t pid;

    pidfile = fopen(pidfile_path, "r");
    if (!pidfile) {
        msg(LOG_ERR, "Error: Could not find eeka pidfile at %s\n", pidfile_path);
        return 1;
    }
    
    if (fscanf(pidfile, "%d", &pid) != 1) {
        msg(LOG_ERR, "Error: Could not read PID from pidfile");
        fclose(pidfile);
        return 1;
    }
    fclose(pidfile);
    
    // Send USR1 signal to toggle
    if (kill(pid, SIGUSR1) == -1) {
        msg(LOG_ERR, "Error sending toggle signal to eeka daemon");
        return 1;
    }
    
    msg(LOG_NOTICE, "Toggle signal sent to eeka daemon (PID %d)", pid);
    return 0;
}

void create_pidfile(void) {
    FILE *pidfile;

    pidfile = fopen(pidfile_path, "w");
    if (pidfile) {
        fprintf(pidfile, "%d\n", getpid());
        fclose(pidfile);
    }
}

void print_usage(const char *progname) {
    printf("Usage: %s [options]\n"
           "Options:\n"
           "  -h, --help              Display this help message\n"
           "  -c, --config <file>     Specify configuration file\n"
           "  -V, --verbose           Enable verbose logging\n"
           "  -t, --toggle            Enable/Disable all button grabs globally\n",
           progname);
}

#define BITS_TO_LONGS(nr) (((nr) + BITS_PER_LONG - 1) / BITS_PER_LONG)
#define BITS_PER_LONG (sizeof(long) * 8)
#define test_bit(nr, addr) (((1UL << ((nr) % BITS_PER_LONG)) & ((addr)[(nr) / BITS_PER_LONG])) != 0)

// Find mouse device function
int find_mouse_device(char *device_path, size_t path_size) {
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        msg(LOG_ERR, "Cannot open /dev/input directory");
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        
        char full_path[280];
        snprintf(full_path, sizeof(full_path), "/dev/input/%s", entry->d_name);
        
        int fd = open(full_path, O_RDONLY);
        if (fd < 0) continue;
        
        // Check if this device has button capabilities
        unsigned long evbit[BITS_TO_LONGS(EV_CNT)] = {0};
        unsigned long keybit[BITS_TO_LONGS(KEY_CNT)] = {0};
        
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) >= 0 &&
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) >= 0) {
            
            // Check for mouse buttons
            if (test_bit(EV_KEY, evbit) && 
                (test_bit(BTN_LEFT, keybit) || test_bit(BTN_RIGHT, keybit))) {
                
                char name[256] = "Unknown";
                ioctl(fd, EVIOCGNAME(sizeof(name)), name);
                msg(LOG_NOTICE, "Found mouse device: %s (%s)", full_path, name);
                
                strncpy(device_path, full_path, path_size - 1);
                device_path[path_size - 1] = '\0';
                close(fd);
                closedir(dir);
                return 0;
            }
        }
        close(fd);
    }
    
    closedir(dir);
    msg(LOG_ERR, "No suitable mouse device found");
    return -1;
}

int init_evdev(void) {
    if (find_mouse_device(evdev_ctx.device_path, sizeof(evdev_ctx.device_path)) < 0) {
        return -1;
    }
    
    evdev_ctx.mouse_fd = open(evdev_ctx.device_path, O_RDONLY | O_NONBLOCK);
    if (evdev_ctx.mouse_fd < 0) {
        msg(LOG_ERR, "Cannot open mouse device %s: %s", evdev_ctx.device_path, strerror(errno));
        return -1;
    }
    
    // Grab exclusive access to prevent events reaching other applications
    if (ioctl(evdev_ctx.mouse_fd, EVIOCGRAB, 1) < 0) {
        msg(LOG_ERR, "Cannot grab exclusive access to %s: %s", evdev_ctx.device_path, strerror(errno));
        close(evdev_ctx.mouse_fd);
        return -1;
    }
    
    msg(LOG_NOTICE, "Grabbed exclusive access to %s", evdev_ctx.device_path);
    return 0;
}

void cleanup_evdev(void) {
    if (evdev_ctx.mouse_fd >= 0) {
        ioctl(evdev_ctx.mouse_fd, EVIOCGRAB, 0);  // Release grab
        close(evdev_ctx.mouse_fd);
        evdev_ctx.mouse_fd = -1;
    }
}

int evdev_button_to_eeka_button(uint32_t button) {
    switch (button) {
        case BTN_LEFT:    return 1;        // LBUTTON
        case BTN_RIGHT:   return 3;       // RBUTTON  
        case BTN_MIDDLE:  return 2;      // MBUTTON
        case BTN_SIDE:    return 8;        // Button8/Back
        case BTN_EXTRA:   return 9;       // Button9/Forward
        case BTN_FORWARD: return 9;     // FBUTTON
        case BTN_BACK:    return 8;        // BBUTTON
        default: 
            msg(LOG_DEBUG, "Unknown button code: 0x%x", button);
            return button;
    }
}

int uinput_fd = -1;

int init_uinput(void) {
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0) {
        msg(LOG_ERR, "Cannot open /dev/uinput: %s", strerror(errno));
        return -1;
    }
    
    // Enable event types
    ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(uinput_fd, UI_SET_EVBIT, EV_REL);
    ioctl(uinput_fd, UI_SET_EVBIT, EV_SYN);
    
    // Enable mouse buttons
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_MIDDLE);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_SIDE);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_EXTRA);
    
    // Enable mouse movement
    ioctl(uinput_fd, UI_SET_RELBIT, REL_X);
    ioctl(uinput_fd, UI_SET_RELBIT, REL_Y);
    ioctl(uinput_fd, UI_SET_RELBIT, REL_WHEEL);
    
    struct uinput_user_dev udev = {0};
    strcpy(udev.name, "eeka virtual mouse");
    udev.id.bustype = BUS_USB;
    udev.id.vendor = 0x1234;
    udev.id.product = 0x5678;
    udev.id.version = 1;
    
    write(uinput_fd, &udev, sizeof(udev));
    ioctl(uinput_fd, UI_DEV_CREATE);
    
    msg(LOG_NOTICE, "Created virtual mouse device");
    return 0;
}

void cleanup_uinput(void) {
    if (uinput_fd >= 0) {
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        uinput_fd = -1;
    }
}

void forward_event(int type, int code, int value) {
    if (uinput_fd < 0) return;
    
    struct input_event ev = {0};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    gettimeofday(&ev.time, NULL);
    
    write(uinput_fd, &ev, sizeof(ev));
    
    // Send sync event
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    write(uinput_fd, &ev, sizeof(ev));
}

void handle_button_press(int button) {
    msg(LOG_DEBUG, "%s pressed", get_button_name(button));
    
    if (!button_state.modifier_pressed && 
        (button == RBUTTON || button == BBUTTON || button == FBUTTON)) {
        
        xcb_window_t target_window = find_target_window(connection);
        WindowClassInfo info = get_window_class_info(connection, target_window);
        
        if (info.valid && is_button_blacklisted(info.instance, info.class_name, button)) {
            msg(LOG_DEBUG, "Button %d pressed on blacklisted window - this shouldn't happen", button);
            return;
        }
        
        if (target_window == XCB_NONE) {
            msg(LOG_DEBUG, "No valid target window found");
            return;
        }
        
        if (button_state.blocked_count < 10) {
            button_state.blocked_buttons[button_state.blocked_count++] = button;
        }
        
        button_state.modifier_pressed = button;
        button_state.combo_used = 0;
        msg(LOG_DEBUG, "Button %d set as potential modifier - blocking original click", button);
        return;
    }

    if (button == LBUTTON && !button_state.modifier_pressed) {
        button_state.modifier_pressed = button;
        button_state.combo_used = 0;
        msg(LOG_DEBUG, "LButton set as potential modifier - allowing passthrough");
        return;
    }

    if (button_state.modifier_pressed && button != button_state.modifier_pressed) {
        msg(LOG_DEBUG, "Detected combo: Button%d + Button%d", button_state.modifier_pressed, button);
        handle_key_binding(button_state.modifier_pressed, button);
        return;
    }
}

int was_button_blocked(int button) {
    for (int i = 0; i < button_state.blocked_count; i++) {
        if (button_state.blocked_buttons[i] == button) {
            return 1;
        }
    }
    return 0;
}

void remove_from_blocked_list(int button) {
    for (int i = 0; i < button_state.blocked_count; i++) {
        if (button_state.blocked_buttons[i] == button) {
            // Shift remaining buttons down
            for (int j = i; j < button_state.blocked_count - 1; j++) {
                button_state.blocked_buttons[j] = button_state.blocked_buttons[j + 1];
            }
            button_state.blocked_count--;
            break;
        }
    }
}

void handle_button_release(int button) {
    msg(LOG_DEBUG, "Button%d released", button);
    
    if (button_state.modifier_pressed == button && was_button_blocked(button)) {
        if (button == LBUTTON) {
            msg(LOG_DEBUG, "LButton modifier released - no action needed");
        } else if (!button_state.combo_used && 
                   (button == RBUTTON || button == BBUTTON || button == FBUTTON)) {
            if (handle_key_binding(button, 0)) {
                msg(LOG_DEBUG, "Found standalone mapping for Button%d", button);
            } else {
                msg(LOG_DEBUG, "No mapping for Button%d - simulating original click", button);
                simulate_button_click(button, find_target_window(connection));
            }
        }
        
        remove_from_blocked_list(button);
        button_state.modifier_pressed = 0;
        button_state.combo_used = 0;
    } else if (was_button_blocked(button)) {
        msg(LOG_DEBUG, "Button %d release (non modifier) - was blocked, removing from blocked list", button);
        remove_from_blocked_list(button);
    } else {
        if (button_state.modifier_pressed == button) {
            button_state.modifier_pressed = 0;
            button_state.combo_used = 0;
        }
        msg(LOG_DEBUG, "Button %d release - was not blocked, ignoring", button);
    }
}

void handle_scroll_event(int scroll_direction) {
    if (button_state.modifier_pressed) {
        msg(LOG_DEBUG, "Detected combo: Button%d + %s", 
            button_state.modifier_pressed, 
            scroll_direction == SCROLL_UP ? "ScrollUp" : "ScrollDown");
        handle_key_binding(button_state.modifier_pressed, scroll_direction);
    } else {
        handle_key_binding(scroll_direction, 0);
    }
}

void simulate_button_click(int button, xcb_window_t target_window) {
    if (target_window == XCB_NONE) {
        msg(LOG_DEBUG, "No target window for click simulation");
        return;
    }
    
    uint8_t xcb_button;
    switch (button) {
        case RBUTTON: xcb_button = XCB_BUTTON_INDEX_3; break;
        case MBUTTON: xcb_button = XCB_BUTTON_INDEX_2; break;
        case BBUTTON: xcb_button = 8; break;
        case FBUTTON: xcb_button = 9; break;
        default:
            msg(LOG_DEBUG, "No click simulation needed for button %d", button);
            return;
    }
    
    xcb_query_pointer_cookie_t cookie = xcb_query_pointer(connection, target_window);
    xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(connection, cookie, NULL);
    
    if (!reply) {
        msg(LOG_DEBUG, "Failed to get pointer position for click simulation");
        return;
    }
    
    int16_t x = reply->win_x;
    int16_t y = reply->win_y;
    free(reply);
    
    xcb_test_fake_input(connection, XCB_BUTTON_PRESS, xcb_button, XCB_CURRENT_TIME, target_window, x, y, 0);
    xcb_flush(connection);
    usleep(10000);
    xcb_test_fake_input(connection, XCB_BUTTON_RELEASE, xcb_button, XCB_CURRENT_TIME, target_window, x, y, 0);
    xcb_flush(connection);
    
    msg(LOG_DEBUG, "Simulated click for button %d at (%d, %d)", button, x, y);
}

void add_to_blacklisted_list(int button) {
    for (int i = 0; i < button_state.blacklisted_count; i++) {
        if (button_state.blacklisted_buttons[i] == button) {
            return;
        }
    }
    if (button_state.blacklisted_count < 10) {
        button_state.blacklisted_buttons[button_state.blacklisted_count++] = button;
    }
}

void remove_from_blacklisted_list(int button) {
    for (int i = 0; i < button_state.blacklisted_count; i++) {
        if (button_state.blacklisted_buttons[i] == button) {
            for (int j = i; j < button_state.blacklisted_count - 1; j++) {
                button_state.blacklisted_buttons[j] = button_state.blacklisted_buttons[j + 1];
            }
            button_state.blacklisted_count--;
            break;
        }
    }
}

int is_currently_blacklisted(int button) {
    for (int i = 0; i < button_state.blacklisted_count; i++) {
        if (button_state.blacklisted_buttons[i] == button) {
            return 1;
        }
    }
    return 0;
}

int are_keyboard_modifiers_pressed(void) {
    xcb_query_keymap_cookie_t cookie = xcb_query_keymap(connection);
    xcb_query_keymap_reply_t *reply = xcb_query_keymap_reply(connection, cookie, NULL);
    
    if (!reply) {
        return 0;
    }
    
    const uint8_t modifier_keycodes[] = {
        XCB_KEY_CONTROL_L,
        XCB_KEY_CONTROL_R,
        XCB_KEY_SHIFT_L,
        XCB_KEY_SHIFT_R,
        XCB_KEY_ALT_L,
        XCB_KEY_ALT_R,
        XCB_KEY_SUPER_L,
        XCB_KEY_SUPER_R,
    };
    
    int modifiers_pressed = 0;
    for (size_t i = 0; i < sizeof(modifier_keycodes) / sizeof(modifier_keycodes[0]); i++) {
        uint8_t keycode = modifier_keycodes[i];
        uint8_t byte_index = keycode / 8;
        uint8_t bit_index = keycode % 8;
        
        if (byte_index < 32 && (reply->keys[byte_index] & (1 << bit_index))) {
            modifiers_pressed = 1;
            msg(LOG_DEBUG, "Keyboard modifier detected (keycode %d)", keycode);
            break;
        }
    }
    
    free(reply);
    return modifiers_pressed;
}

void process_evdev_events(void) {
    struct input_event events[64];
    ssize_t bytes = read(evdev_ctx.mouse_fd, events, sizeof(events));
    
    if (bytes < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            msg(LOG_ERR, "Error reading from mouse device: %s", strerror(errno));
        }
        return;
    }
    
    size_t num_events = bytes / sizeof(struct input_event);
    
    for (size_t i = 0; i < num_events; i++) {
        struct input_event *ev = &events[i];
        
        if (!enabled || !grabbing_enabled) {
            forward_event(ev->type, ev->code, ev->value);
            continue;
        }
        
        if (ev->type == EV_KEY) {
            int eeka_button = evdev_button_to_eeka_button(ev->code);
            int should_block = 0;
            int is_blacklisted = 0;
            
            if (eeka_button == RBUTTON || eeka_button == BBUTTON || eeka_button == FBUTTON || eeka_button == LBUTTON) {
                if (are_keyboard_modifiers_pressed()) {
                    msg(LOG_DEBUG, "Keyboard modifiers detected - passing button %d through", eeka_button);
                    forward_event(ev->type, ev->code, ev->value);
                    continue;
                }
            }
            
            if (ev->value == 1) { // PRESS
                if (eeka_button == RBUTTON || eeka_button == BBUTTON || eeka_button == FBUTTON) {
                    xcb_window_t target_window = find_target_window(connection);
                    WindowClassInfo info = get_window_class_info(connection, target_window);
                    
                    if (info.valid && is_button_blacklisted(info.instance, info.class_name, eeka_button)) {
                        is_blacklisted = 1;
                        add_to_blacklisted_list(eeka_button);
                        msg(LOG_DEBUG, "Button %d on blacklisted window - passing through completely", eeka_button);
                    } else {
                        should_block = 1;
                    }
                }
                
                if (!is_blacklisted) {
                    handle_button_press(eeka_button);
                }
                
            } else if (ev->value == 0) { // RELEASE
                if (is_currently_blacklisted(eeka_button)) {
                    is_blacklisted = 1;
                    remove_from_blacklisted_list(eeka_button);
                    msg(LOG_DEBUG, "Button %d release - was blacklisted, passing through", eeka_button);
                } else {

                    if (was_button_blocked(eeka_button)) {
                        should_block = 1;
                    }
                    handle_button_release(eeka_button);
                }
            }
            
            if (!should_block) {
                forward_event(ev->type, ev->code, ev->value);
            }
            
        } else if (ev->type == EV_REL && ev->code == REL_WHEEL) {
            int should_block = 0;
            
            if (are_keyboard_modifiers_pressed()) {
                msg(LOG_DEBUG, "Keyboard modifiers detected - passing scroll through");
                forward_event(ev->type, ev->code, ev->value);
                continue;
            }
            
            if (button_state.modifier_pressed) {
                should_block = 1;
            }
            
            if (ev->value > 0) {
                handle_scroll_event(SCROLL_UP);
            } else if (ev->value < 0) {
                handle_scroll_event(SCROLL_DOWN);
            }
            
            if (!should_block) {
                forward_event(ev->type, ev->code, ev->value);
            }
            
        } else {
            forward_event(ev->type, ev->code, ev->value);
        }
    }
}

int main(int argc, char *argv[]) {
    const char *config_path = "/etc/eeka.conf";
    int opt;
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {"verbose", no_argument, 0, 'V'},
        {"toggle", no_argument, 0, 't'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hc:Vt", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'c':
                config_path = optarg;
                break;
            case 'V':
                verbose = 1;
                break;
            case 't':
                return toggle_eeka_daemon();
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    create_pidfile();
    parse_config_file(config_path);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, toggle_signal_handler);

    connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(connection)) {
        msg(LOG_ERR, "Cannot connect to X server");
        return EXIT_FAILURE;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    if (!screen) {
        msg(LOG_ERR, "Cannot get screen");
        xcb_disconnect(connection);
        return EXIT_FAILURE;
    }

    if (init_evdev() < 0) {
        msg(LOG_ERR, "Failed to initialize evdev");
        xcb_disconnect(connection);
        return EXIT_FAILURE;
    }
    
    if (init_uinput() < 0) {
        msg(LOG_ERR, "Failed to initialize uinput");
        cleanup_evdev();
        xcb_disconnect(connection);
        return EXIT_FAILURE;
    }

    msg(LOG_NOTICE, "eeka started successfully");
    
    int xcb_fd = xcb_get_file_descriptor(connection);
    
    while (running) {
        struct pollfd fds[2];
        fds[0].fd = xcb_fd;
        fds[0].events = POLLIN;
        fds[1].fd = evdev_ctx.mouse_fd;
        fds[1].events = POLLIN;
        
        int poll_result = poll(fds, 2, 100);
        
        if (poll_result < 0) {
            if (errno == EINTR) continue;
            msg(LOG_ERR, "poll() failed: %s", strerror(errno));
            break;
        }
        
        if (fds[0].revents & POLLIN) {
            xcb_generic_event_t *event;
            while ((event = xcb_poll_for_event(connection)) != NULL) {
                free(event);
            }
        }
        
        if (fds[1].revents & POLLIN) {
            process_evdev_events();
        }
    }

    cleanup_evdev();
    cleanup_uinput();
    
    if (connection) {
        xcb_disconnect(connection);
    }

    unlink(pidfile_path);
    msg(LOG_NOTICE, "eeka terminated");
    return EXIT_SUCCESS;
}

void msg(int priority, const char* format, ...) {
    
    if (!verbose && priority > LOG_WARNING) return;
    
    va_list args;
    va_start(args, format);
    time_t now;
    struct tm *tm_info;
    char timestamp[26];
    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    const char* priority_str;
    switch (priority) {
        case LOG_DEBUG: priority_str = "DEBUG"; break;
        case LOG_NOTICE: priority_str = "NOTICE"; break;
        case LOG_WARNING: priority_str = "WARNING"; break;
        case LOG_ERR: priority_str = "ERROR"; break;
        default: priority_str = "INFO"; break;
    }
    fprintf(stdout, "%s [%s] ", timestamp, priority_str);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(args);
}
