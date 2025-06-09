# eeka

The reason i created this program was to get functionality i was able to achieve on my **2013**
windos rice using a [dirthack AutoHotkey script](https://gist.github.com/budRich/6044613):  

```AutoHotkey
/* dirthack.ahk */

RButton & WheelUp::Send,^{PGDN}
RButton & WheelDown::Send,^{PGUP}
RButton & MButton::Send,^w
```

More or less all applications with "tabs" (browsers, filemanagers, text editors) have support for keyboard 
shortcuts `Ctrl+PgUp/PgDown` to navigate the tabs (also `Ctrl+[shift]+Tab`) and `Ctrl+w` to close the tabs. It is really sweet to have that functionality also on the mouse, and i set it up so that holding Right Mousebutton while scrolling the mousewheel. I have never managed to get this working on linux and X11, since it would require using the RightButton of the mouse as a "modifier" in a keyboard shortcut. But now, with `eeka` it is possible :D

```
eeka --help
Usage: eeka [options]
Options:
  -h, --help              Display this help message
  -c, --config <file>     Specify configuration file
  -V, --verbose           Enable verbose logging
  -t, --toggle            Enable/Disable all button grabs globally
```

```
# ~/.config/eeka/config

RButton & ScrollUp   = Ctrl+PageDown
RButton & ScrollDown = Ctrl+PageUp
RButton & MButton    = Ctrl+W

LButton & ScrollUp   = Super+Tab
LButton & ScrollDown = Super+Shift+Tab
LButton & RButton    = Super+M

BButton = Backspace
FButton = F5

window [instance=i3-frame] {
    blacklist = RButton
}

window [instance=Shattered Pixel Dungeon] {
    blacklist = RButton
}

window [class=Code] {
    
    RButton & LButton = Ctrl+B
}

window [class=Sublime_text] {
    Button8 = Backspace
    RButton & LButton = Ctrl+Alt+S
}

window [instance=brave-browser, class=bloatlord] {
    RButton & MButton    = Ctrl+D
    RButton & LButton    = Alt+X
    RButton & ScrollUp   = Ctrl+K
    RButton & ScrollDown = Ctrl+J
}
```

With `eeka` you can use Button1, Button3, Button8 and Button9 as modifiers (i.e Left, Right, Back and Forward button). Button1/LButton will behave slightly different by always passing the button event through on press, to not mess up normal drag and click functionality. But on the other buttons, normal behaviour of the button is instead sent as a "fake" click when the button has been released without being used as a modifier. This is needed for Button3/RButton, otherwise context menu will popup as soon as you press, which is not desired when you want to use it as a modifier. This however do **mess up Right button dragging** which is used in some games and advanced graphic programs like blender. So for programs where grabbing the buttons causes problems, button blacklists can be added to **window rules**.  

It is also possible to *disable* all grabbing on a running instance of `eeka` by either sending it **USR1** signal, or execute `eeka --toggle` so it can be a good idea to bind that to global keybinding in f.i. i3wm or sxhkd or something.

## installing

- eeka only works on X11 (uses [xcb] for *window rules*).
- eeka only works on Linux (uses [evdev] for fine grained control over mousebutton grabs)

But beside [xcb] it doesn't have any dependencies, so it should be trivial to build on any linux distribution. 

```
$ make
# make install
```

**Requirements:**
- Linux with X11
- xcb development headers (usually `libxcb-dev` or `libxcb-devel`)

## Copy~~right~~left

eeka was developed by budRich, spring 2025 and released under the BSD Zero Clause License.

As of writing this I have only tested it on Arch and i3wm, but eeka should work on any linux with xorg. If you find any issues with the program, please report them!

[xcb]: https://xcb.freedesktop.org/
[evdev]: https://www.freedesktop.org/wiki/Software/libevdev/
