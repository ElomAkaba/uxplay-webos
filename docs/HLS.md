# YouTube HLS playback on older webOS TVs

The YouTube iOS app's AirPlay button sends HLS video rather than the mirroring
buffer stream. This change registers UxPlay's HLS callbacks and loads its local
playlist through the native Starfish player. The webOS 3 load API expects a URI
string followed by an options object. The master playlist is restricted to H.264
up to 720p, and local playlist URLs use IPv4 loopback. OpenSSL uses its device
random source, allowing startup in jails that expose `/dev/urandom` but not
`/dev/random`.

## Separate audio on webOS 3

YouTube offers video and audio as separate HLS renditions. LG documents that
`EXT-X-MEDIA` is not supported on webOS 3.5 or earlier:
<https://webostv.developer.lge.com/develop/specifications/streaming-protocol-drm>.
Direct playback on those versions can show video without audio.

The optional Python/FFmpeg companion merges the H.264 and AAC streams into a
single MPEG-TS HLS stream using stream copy. A Raspberry Pi or another Linux
host on the same LAN can run it. Mirroring does not require this companion.
DRM-protected streaming services are outside the scope of this change.

1. Install Python 3 and FFmpeg on the companion host. The tested FFmpeg provides
   the HLS demuxer options `allowed_segment_extensions` and `extension_picky`.
2. Copy `companion/server.py` to the host and create a dedicated writable state
   directory. Copy `companion/config.example.json` and replace the documentation
   addresses with your TV and companion IPv4 addresses. The public URL must
   match the configured bind address and port.
3. Generate a token with `python3 -c 'import secrets; print(secrets.token_hex(32))'`
   and save it in the configured token file. Restrict access to the service user.
4. Run `python3 server.py --config /path/to/config.json`. A systemd unit template
   is provided; create its service user and directories before enabling it.
5. In the TV app's private `state/hls-bridge.conf`, put the companion IPv4 address,
   port, and matching token on three separate lines. The file must be readable
   by the native app user and should have mode 0600. For example:

   ```text
   192.0.2.20
   8878
   <your 64-character hexadecimal token>
   ```

6. Reopen UxPlay webOS, then select it from YouTube's AirPlay menu.

A missing configuration leaves direct HLS enabled. An invalid configuration or
an unreachable configured bridge reports a playback failure instead of silently
falling back to video without audio. Remove the configuration to disable the
bridge. The companion accepts one active stream; starting a new stream replaces
its temporary files. Keep the service on a trusted LAN. Its start endpoint uses
the token; output URLs contain random session IDs. Do not expose the HTTP service
to the internet. Source playlists and FFmpeg logs can contain signed media URLs
and must not be published.

## Validation and remaining work

On an LG 55UH615V-ZB, firmware 05.70.50, SDK 3.4.0:

- The receiver starts and pairs with an iPhone.
- Direct YouTube AirPlay video reaches the screen after the native load and
  playlist compatibility fixes.
- A local H.264/AAC HLS test loads and enters the native playing state.
- FFprobe detects 1280×720 H.264 plus 44.1 kHz stereo AAC in the companion output;
  the TV detects both tracks and reports load completed and playing.

Audible companion audio has not yet been confirmed by the person at the TV.
Pause, seeking, repeated track changes and long sessions need acceptance testing.
The refactored configurable bridge is covered by host tests and the ARM build;
it has not been redeployed to the validation TV. Existing mirroring tests remain
part of `tools/test.sh`, including ASan/UBSan.

The pinned upstream changes are stored in `patches/uxplay-webos-hls.patch` and
applied by `tools/bootstrap.py`. They are included in the corresponding source
archive rather than depending on an untracked edit in `vendor/`.
