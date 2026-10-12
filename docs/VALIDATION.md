# UxPlay webOS validation on LJ600B

Hardware inspection and tests started 2026-09-06. This record distinguishes user-observed playback from protocol/log checks.

## Target and environment

- LG 32LJ600B-SA, firmware 06.10.60; `/etc/starfish-release`: Rockhopper 3.9.3-63015.
- ARMv7 Cortex-A9, two cores, kernel 4.4.3; glibc 2.19; GStreamer 1.8.0; 680832 KiB system RAM.
- Native application context through the launcher, without a background root service.
- Build host: Apple Silicon macOS 26.6.2; GCC 12.2 webOSbrew target toolchain.

## Confirmed

| Test | Evidence |
| --- | --- |
| Native H.264 + PCM | User confirmed moving image and sound; 450 video access units + 1500 PCM blocks, `PLAYING`, EOS and successful teardown |
| AirPlay discovery | macOS `dns-sd` found `UxPlay webOS` on `_airplay._tcp.local` |
| PIN pairing | On-screen PIN accepted; paired key persisted, subsequent process startup reported `paired=1` |
| Mac mirroring | User confirmed stable video; an initial live-meeting check had good perceived lip-sync |
| Mac audio continuity | User confirmed continuous audio with the Mac output routed to UxPlay webOS |
| Mac extended display | User confirmed video and audio; pointer movement has perceptible latency, so this is not certified for interactive low-latency use |
| Mac extended-display soak | 30:00 continuous session: 61 samples, one PID, RSS 40.1–45.2 MiB, no sample failures, queue overflow or audio errors |
| Mac reconnect cycles | User completed 10 disconnect/reconnect cycles, alternating TV Back and stopping from macOS; playback and receiver recovery were reported working throughout |
| iPad mirroring | User confirmed image; an earlier interactive audio check completed after the PCM-track fix |
| iPad continuous-audio path | User heard an uninterrupted controlled 880 Hz tone. Receiver telemetry accepted 3,135 AAC-ELD packets with zero late packets, queue overflow or audio decoder errors |
| iPad video soak | Manual 30-minute continuous mirroring session on 2026-09-07; video remained active and the user observed no crash, disconnect, or playback interruption. This session was not captured by the RSS sampler. |
| iPad rotation | User confirmed portrait/landscape transitions preserve the image and audio |
| iPad Wi-Fi interruption and recovery | With active mirroring, disabling Wi-Fi left the image frozen for about 10 seconds before `connection lost`; after Wi-Fi was restored, selecting Connect again resumed normally |
| Decoder throughput | TV decoder logs reported 29–30 frames/s during the iPad session |
| Host parsing/decoding | ASan + UBSan: landscape/portrait SPS, 10,000 malformed inputs, PCM checks, AAC-LC to stereo PCM; passed |
| ELF preflight | ARM32 soft-float, required glibc <= 2.19; private libstdc++ supplies required C++ versions |
| TV dynamic loader | All reported libraries resolved; private libstdc++ was loaded from the app directory |

`FRAMEREADY` callbacks are throttled by the platform and are not a per-frame FPS counter. Use the decoder's throughput logs or an external recording for actual frame rate.

## In progress / not yet certified

- Instrumented 30-minute iPad session with RSS and receiver telemetry, to complement the completed manual video soak.
- Faster user feedback while a sender network interruption is being detected: the current behavior holds the last image for about 10 seconds before showing `connection lost`.
- Quantified end-to-end latency and audio/video offset <= 100 ms. The user reported improved but still visible delay; no instrumented latency result is claimed.

Raw local evidence is under ignored `build/logs/`. `tools/soak.py --label ipad` samples PID/RSS and session counters every 30 seconds for 30 minutes. A completed sampler is not sufficient by itself: the video must remain active and the user must confirm playback.

## Repeatable manual acceptance

1. Open the app, connect the sender, enter a PIN for a new device, and play unprotected video with audio.
2. Rotate an iPad both ways; on Mac test mirroring and extended display, selecting TV audio output when necessary.
3. Run `tools/soak.py --label ipad` or `--label mac` while playing continuously. Verify no process replacement, queue overflow or decoder failures, and inspect the RSS trend after warm-up.
4. Disconnect and reconnect ten times, alternating Back on the TV and stopping from the sender. Verify that the launcher, TV remote and other TV playback remain usable afterward.
5. Disable Wi-Fi on the sender briefly, re-enable it and reconnect. Do not change the TV's networking during a remote SSH maintenance session.
6. Generate `dist/sync-test.mp4` with `tools/make-sync-video.sh`. Record the sender and TV together with an external camera at a known frame rate. Count the difference between corresponding flashes for video latency; compare flash and tone arrival to assess A/V offset. The source-frame resolution is 33.3 ms; the file contains bright flashes and tones, once per second.

## Known implementation boundaries

- Starfish's buffering contributes latency on this older platform. Stability is prioritized over gaming latency.
- Queue overflow, a >3-second player stall, or a large timestamp discontinuity produces a recoverable disconnect and an on-screen instruction to reconnect.
- Paired devices are stored in the app folder; app reinstallation/data removal can reset them. The development atomic-update helper preserves them. Moving from the old development app ID to `com.github.danlflh.uxplaywebos` requires manual removal of the old build and starts a new pairing state.
- Unsupported HLS, DRM content and HEVC/HDR are not acceptance targets.

## Additional webOS 3 HLS work

The YouTube HLS path has additional validation on an LG 55UH615V-ZB, firmware
05.70.50, SDK 3.4.0. See [HLS validation and remaining work](HLS.md) for the
observed video result, separate-audio workaround and unverified scenarios.
This does not expand the stable mirroring acceptance claims above.
