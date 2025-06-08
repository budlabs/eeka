# eeka
Mouse button remapper for X11

The reason i created this program was to get functionality i was able to achieve on my old
windos rice using a dirthack AutoHotkey script:  

```AutoHotkey
/* dirthack.ahk */

RButton & WheelUp::Send,^{PGDN}
RButton & WheelDown::Send,^{PGUP}
RButton & MButton::Send,^w
```

More or less all applications with "tabs" (browsers, filemanagers, text editors) have support for keyboard 
shortcuts `Ctrl+PgUp/PgDown` to navigate the tabs (also `Ctrl+\[shift\]+Tab`) and `Ctrl+w` to close the tabs. It is really sweet to have that functionality also on the mouse, and i set it up so that holding Right Mousebutton while scrolling the mousewheel. I have never managed to get this working on linux and X11, since it would require using the RightButton of the mouse as a "modifier" in a keyboard shortcut. But now, with `eeka` it is possible :D

```
# ~/.config/eeka

Button3 & ScrollUp = Ctrl+PageUp
Button3 & ScrollDown = Ctrl+PageDown
Button3 & Button2 = Ctrl+W

Button8 = Backspace # remap back button to Backspace
Button9 = F5        # remap forward button to F5

blacklist = [instance=Shattered Pixel Dungeon], \
            [instance=sublime_main]

window [class=Code] {
    Button8 & ScrollDown = Ctrl+B
    Button8 & ScrollUp = Ctrl+B
}

window [instance=brave-browser, class=bloatlord] {
    Button3 & Button2 = Ctrl+D
    Button8 & ScrollDown = Alt+x
    Button8 & ScrollUp = Alt+x
}
```

Above is my personal configuration, as you can see i also have remapped back and forward button. This is something i was able to do with i3 and `xdotool`:

```
# ~/.config/i3/config

bindsym --whole-window button8 exec --no-startup-id exec xdotool key BackSpace
bindsym --whole-window button9 exec --no-startup-id exec xdotool key F5
```

With `eeka` you can use Button1, Button3, Button8 and Button9 as modifiers (i.e Right, Back and Forward button). 

This is also true for right mouse button in some applications (blender, games like Shattered Pixel Dungeon) where right mousebutton can be used to pan the screen, i.e. dragging with right button.

It is also possible to execute `eeka` with `eeka -t` to toggle the application temporarly (till you execute `eeka -t` again).

Another issue i noticed while building this, was that since i had remapped `Ctrl+W` in my browser because of reasons, closing tabs with Rbutton+MButton didn't work in that application, so i also added "*window rules*" in which you can define override remappings that will only apply to the windows matching the criteria. 

