# Changelog

All notable changes to this project are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

- Add optional launcher return behavior, configurable AirPlay naming and a hidden-backend bundle for Lumen.

- Add experimental YouTube HLS playback through the native webOS player.
- Restrict HLS variants to H.264 up to 720p and rewrite loopback playlist URLs.
- Add an optional configurable FFmpeg companion for webOS 3 separate-audio streams.
- Fix OpenSSL random seeding in older app jails and initialize HLS language selection.
- Apply pinned UxPlay compatibility changes during bootstrap and include them in source archives.

## [0.1.0] - 2026-09-07

### Added

- Experimental native AirPlay receiver for rooted webOS TVs.
- H.264 mirroring, stereo PCM/AAC/ALAC audio handling, PIN pairing, AirPlay discovery, and adjustable audio delay.
- Reproducible local build, ABI verification, source-package generation, sanitizer-backed host tests, and release documentation.

### Known limitations

- Validated only on the LG 32LJ600B-SA with firmware 06.10.60.
- DRM/HLS, HEVC/HDR, recording, startup at boot, and broad-device compatibility are unsupported or unverified.
