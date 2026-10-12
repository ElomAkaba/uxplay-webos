# Frontend integration

`AirPlayPanel.tsx` is the Lumen frontend component used with the native backend.
Copy it into the launcher's `src/components/` directory. It uses Lumen's existing
key handlers, Tile/Glyph styling and Luna helpers.

Add `AIRPLAY_ID = 'com.github.danlflh.uxplaywebos'` to the launcher constants and
an `airplay` Glyph (a display outline with a triangle). Put an AirPlay item in
the top bar. Opening that item shows the panel; while it is open, disable the
Home screen's key handlers. Exclude `AIRPLAY_ID` from app discovery so the
backend does not appear as an app tile.

The receiver name in the panel is Lumen TV, matching the default bundle name.
If a bundle uses a different receiver name, update the panel text as well.
Use the existing glass panel style and a full-width Start button. Native
receiver setup and lifecycle limits are described in [LUMEN.md](../../docs/LUMEN.md).
