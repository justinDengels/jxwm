# JXWM - Justin's X Window Manager
Hello, if your seeing this this is my attempt at making an X window manager. I know a million people have done it already and "X is dead", but I don't care, it's interesting to me, and I like the idea of using something I myself made.

Currently it's a jumbled mess of a bunch of the things I want to implement but haven't been properly implemented yet and isn't really usable at the moment. Below I'll list the ideas I have and the things I want to implement. I've never really done any kind of a medium-big project before so I'm a little lost, but I'd appreciate any help/advice I get. 

# Vision for the window manager
I want this to be a minimalist window manager like dwm, but with a little more. I'd like it to be a little nicer dwm that lets you put in whatever you want (ie you can use polybar and not just be forced to use the dwm bar).

Things I want to implement:
+ Tiling (currently have something for master/stack, maybe more down the line)
+ Tags/Workspaces (Ability to move windows across workspaces/moving windows in general)
+ Support for external bars (like polybar, I don't care to make my own)
+ Config file that can be reloaded with a keybinding
+ Single Monitor Support (As I only use a single monitor I'm not interested in multi-monitor support for the time being, but it could be implemeneted later down the line if everything else is finished and there's request for it)
+ XCB (Currently using XLib, will switch to xcb later down the line)

This is probably a terrible README but those are just my thoughts, if you see this thanks for reading and I'd appreciate any suggestions. I'd also appreciate any resources you found helpful.
