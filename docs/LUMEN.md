# Lumen launcher integration

The native receiver can act as a hidden backend for a web launcher. A launcher
shows an AirPlay action rather than exposing another app tile. The action opens
the receiver, which advertises a configurable name and returns to the launcher
when Back is pressed or a completed playback session ends.

Put two lines in the receiver's private `state/launcher.conf`:

```text
com.example.launcher
Lumen TV
```

The first line is the launcher application ID; the second is the AirPlay display
name. Use a valid application ID and a name of up to 60 bytes. Keep the file
readable by the receiver's app user, with mode 0600. Removing it restores the
standalone behavior. Existing pairing keys are reused.

Build a combined distribution with:

```sh
tools/build.sh
python3 tools/create-lumen-bundle.py \
  --launcher-ipk /path/to/launcher.ipk \
  --launcher-id com.example.launcher
```

`dist/lumen-airplay-bundle.zip` contains both IPKs and the launcher configuration.
Its backend manifest uses `visible:false`; the frontend should also exclude the
backend's ID from its own app discovery. Starting it still uses the normal
applicationManager launch API. The optional HLS audio bridge uses its separate
configuration described in [HLS.md](HLS.md).

## Behavior and validation

On the tested webOS 3 TV, opening a web launcher causes the native receiver to
exit. This integration therefore starts AirPlay on demand. It does not promise
an always-on background AirPlay target or replace native playback with browser
playback. The configured name is visible while the receiver is open.

Verified on LG 55UH615V-ZB, firmware 05.70.50:

- The AirPlay action in the web launcher starts the native receiver.
- The receiver's waiting screen uses the configured Lumen TV name.
- Pressing Back returns to the configured launcher.
- The native backend is excluded from the launcher's app list.

Returning after an actual iPhone streaming session still requires acceptance
testing. A system-driven exit, such as switching to a different app or powering
off, does not request a return to the launcher. Configuration parsing and
application-ID validation are included in `tools/test.sh`.
