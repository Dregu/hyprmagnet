# hyprmagnet

Hyprland plugin with some smart mouse hooks, for Hyprland 0.53+.

- move mouse events from just outside a tiled window (i.e. from outer gaps and borders) to just inside the window, to click browser tabs and scrollbars without having to aim inside the window
- smarter warp to nearby monitors when bonking non-intersecting monitor edges

```lua
-- Enabled per window with dynamic tag magnet
hl.window_rule{ match = { class = 'firefox|codium' }, tag = '+magnet' }

hl.config{
  plugin = {
    magnet = {
      edge  = 'tr', -- Window edges to enable on, any combo of t/b/l/r (default: t)
      pad   = 3,    -- Pad inside the window if your tabs aren't quite at the edge (default: 0)
      warp  = 'ig', -- Warp flags when bonking monitor edges, any combo of i/g (default: none)
                    -- i: warp to nearest intersection, g: cross gaps in monitor layout
                    -- Order matters; ig prefers intersecting monitors and gi prefers gaps
      delay = 500   -- Cooldown between monitor warps, ms (default: 500)
    }
  }
}
```