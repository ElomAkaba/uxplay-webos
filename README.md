# UxPlay webOS

An experimental native AirPlay mirroring receiver for rooted LG webOS TVs. Open **UxPlay webOS** from the TV launcher, select it from Screen Mirroring on an iPhone, iPad, or Mac, then enter the on-screen PIN on first connection. No separate receiver computer is needed.

## Status and validated hardware

This is an experimental `0.1.0` release. It has been validated on an LG **32LJ600B-SA**, firmware **06.10.60**, Rockhopper **3.9.3**, with an ARMv7 Cortex-A9 target. The proven target is H.264 at up to 1280×720 / 30 fps with stereo audio. See [the validation record](docs/VALIDATION.md) for evidence, unverified scenarios, and the repeatable manual acceptance procedure.

## Requirements

- A rooted compatible LG webOS TV with the webOS Brew development environment.
- The TV and sender on the same LAN with multicast/mDNS available.
- For local builds: Apple Silicon macOS, Xcode command-line tools, Git, CMake, Ninja, the webOS TV CLI (`ares-package`, `ares-install`, `ares-launch`), and FFmpeg. The supplied toolchain currently targets macOS ARM64.

## Install and use

Build the package locally, then configure a neutral device name with `ares-setup-device`:

```sh
tools/build.sh
TV_DEVICE=your-tv tools/tv.sh install
TV_DEVICE=your-tv tools/tv.sh launch
```

The package is `dist/com.github.danlflh.uxplaywebos_0.1.0_arm.ipk`. Changing from the former development ID requires manually removing that old build; pairing data belongs to the app ID, so it starts over after the identity change.

On the TV, open **UxPlay webOS**. Start Screen Mirroring from an iPhone, iPad, or Mac and select **UxPlay webOS**. Enter the PIN when prompted. On macOS, select **UxPlay webOS** as the audio output if it is not selected automatically.

- Left/Right before connecting changes audio delay in 25 ms steps (−1000 to +1000 ms); positive values play audio later. The value is saved locally.
- Back during a session disconnects and returns to the waiting screen. Back while waiting, or Exit, closes the app.
- One transmitting device is accepted at a time.

For direct SSH helpers, explicitly set the connection details—no personal network, TV alias, or key path is embedded:

```sh
export TV_DEVICE=your-tv
export TV_HOST=tv.example.lan
export TV_KEY=/absolute/path/to/private-key
tools/tv.sh logs
tools/tv.sh loader-check
```

`tools/tv.sh update-binary` replaces only the executable and intentionally preserves current pairing data. Use a full install whenever packaged libraries change.

## Reproducible build and tests

`packaging/appinfo.json` is the single version source. `tools/build.sh` downloads checksum-pinned inputs, builds private dependencies, cross-compiles the receiver, verifies the ELF ABI, and packages the IPK. The generated dependencies, build tree, logs, pairing state, and `dist/` artifacts are excluded from Git.

```sh
tools/test.sh              # H.264, malformed input, PCM/AAC and ASan/UBSan
tools/probe.sh             # package a 15-second native playback probe
tools/source-package.py    # corresponding source archive
python3 tools/validate-metadata.py
```

The corresponding source archive is `dist/uxplay-webos-0.1.0-source.tar.gz`; it includes this source tree, pinned UxPlay source, original dependency archives, and build instructions. Final packaging remains a local Apple Silicon task because of the current toolchain.

## Launcher integration

For an on-demand AirPlay action inside Lumen and a hidden native backend, see
[launcher integration](docs/LUMEN.md). The combined bundle includes the launcher
and backend IPKs. Standalone operation remains the default.

## Architecture

`src/receiver.cpp` integrates the pinned UxPlay core and handles discovery, PIN pairing, and bounded packet queues. `src/main.cpp` owns session lifecycle and timing. `src/media.cpp` feeds the TV hardware decoder through Starfish Media Pipeline and ACB, while FFmpeg decodes AAC-ELD, AAC-LC, ALAC, and PCM to stereo PCM. SDL owns the window and remote input while the hardware video plane remains visible.

Queues are capped at 4 MiB/512 packets. Decoder stalls, timestamp discontinuities, and queue overflow end the current session rather than allowing uncontrolled memory growth.

## Privacy, security, and logs

Pairing data, receiver identity, and the selected audio delay are stored only in the app's private `state/` directory. Up to 64 paired client keys are retained. Reinstalling or removing app data resets that state. The receiver has no cloud service, analytics, or outbound account connection.

Diagnostics are written to `uxplay.log`, rotated around 1 MiB with one previous file. Exporting logs does not copy pairing files. Do not publish logs without review: sender names and local network details may appear in them. The package contains no SSH keys, logs, or pairing data.

## Supported features and limitations

Experimental YouTube playback from the in-app AirPlay button is also available; see [HLS setup](docs/HLS.md). On webOS 3, separate video/audio tracks require the optional companion bridge.

Supported: AirPlay discovery, PIN pairing, H.264 mirroring, stereo audio, iPad rotation, reconnecting, and adjustable audio delay.

Not supported or not certified: DRM playback, HEVC/HDR, recording, startup at boot, broad TV/firmware compatibility, Mac extended display, and low-latency gaming. Starfish buffering on this older platform can add visible latency.

## Troubleshooting

- **The receiver does not appear:** confirm both devices use the same LAN and that multicast/mDNS is not blocked.
- **No sound on a Mac:** choose **UxPlay webOS** in macOS audio output, reconnect, then adjust audio delay if needed.
- **Old app still appears or pairing changed:** remove the old development-ID installation manually; this release intentionally uses a new app ID.
- **Package or loader failure:** run `tools/tv.sh loader-check` with `TV_HOST` and `TV_KEY` set, and inspect `tools/tv.sh logs`.

## Roadmap and contributing

Before a stable release, the project needs longer soak tests, quantified latency, more TV/firmware coverage, Mac audio and extended-display validation, and repeated reconnect/network-recovery testing. Contributions are welcome as focused issues and pull requests with reproducible evidence. Please run `tools/test.sh` and `python3 tools/validate-metadata.py` before opening a pull request.

## Credits and license

UxPlay webOS is maintained by Daniel Filho ([@danlflh](https://github.com/danlflh)). It builds on FDH2/UxPlay, llhttp, OpenSSL, libplist, FFmpeg, the GCC Runtime, the webOS Brew native toolchain and samples, and technical reference from mariotaku/ss4s. SDL2, SDL2_ttf, Starfish Media Pipeline, and ACB are supplied by the TV. Full notices are in [THIRD_PARTY.md](THIRD_PARTY.md).

Licensed under [GPL-3.0-or-later](LICENSE). This is an independent project and is not affiliated with, endorsed by, or supported by Apple or LG.
