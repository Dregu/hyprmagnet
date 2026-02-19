# hyprmagnet

Plugin to move mouse events from just outside a tiled window (i.e. from outer gaps and borders) to just inside the window, to click browser tabs and scrollbars without having to aim inside the window, for Hyprland 0.53+.

```ini
# Enabled per window with dynamic tag magnet
windowrule = match:class firefox|codium, tag +magnet

plugin {
    magnet {
        edge = tr # Window edges to enable on, any combination of t/b/l/r (default: t)
        pad  = 3  # Padding inside the window if your tabs aren't quite at the edge (default: 0)
    }
}
```
