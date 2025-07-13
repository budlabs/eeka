#pragma once

#include <syslog.h>

extern int verbose;

void msg(int priority, const char* format, ...);

#define XK_Escape       0xff1b
#define XK_Return       0xff0d
#define XK_Tab          0xff09
#define XK_BackSpace    0xff08
#define XK_Delete       0xffff
#define XK_Page_Up      0xff55
#define XK_Page_Down    0xff56
#define XK_space        0x0020
#define XK_F1           0xffbe
#define XK_F2           0xffbf
#define XK_F3           0xffc0
#define XK_F4           0xffc1
#define XK_F5           0xffc2
#define XK_F6           0xffc3
#define XK_F7           0xffc4
#define XK_F8           0xffc5
#define XK_F9           0xffc6
#define XK_F10          0xffc7
#define XK_F11          0xffc8
#define XK_F12          0xffc9
#define XK_Left         0xff51
#define XK_Up           0xff52  
#define XK_Right        0xff53
#define XK_Down         0xff54

#define MOD_SHIFT 0x01
#define MOD_CTRL  0x02
#define MOD_ALT   0x04
#define MOD_SUPER 0x08

#define XCB_KEY_CONTROL_L   37
#define XCB_KEY_CONTROL_R   105
#define XCB_KEY_SHIFT_L     50
#define XCB_KEY_SHIFT_R     62
#define XCB_KEY_ALT_L       64
#define XCB_KEY_ALT_R       108
#define XCB_KEY_SUPER_L     133
#define XCB_KEY_SUPER_R     134
#define XCB_KEY_META_L      XCB_KEY_ALT_L    // Meta is usually the same as Alt
#define XCB_KEY_META_R      XCB_KEY_ALT_R

typedef enum {
    LBUTTON     = 1,
    MBUTTON     = 2,
    RBUTTON     = 3,
    SCROLL_UP   = 4,
    SCROLL_DOWN = 5,
    BUTTON6     = 6,
    BUTTON7     = 7,
    BBUTTON     = 8,
    FBUTTON     = 9 
} MouseButton;

typedef struct {
    int mouse_fd;
    char device_path[256];
} EvdevContext;

typedef struct {
    int modifier_pressed;
    int combo_used;
    int blocked_buttons[10];
    int blocked_count;
    int blacklisted_buttons[10];
    int blacklisted_count;
} ButtonState;

typedef struct {
    char instance[256];
    char class_name[256];
    int valid;
} WindowClassInfo;

