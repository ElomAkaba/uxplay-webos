#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
ffmpeg -hide_banner -loglevel error -y -f lavfi -i testsrc2=size=1280x720 -frames:v 1 -c:v libx264 -profile:v baseline -f h264 build/tests/landscape.h264
ffmpeg -hide_banner -loglevel error -y -f lavfi -i testsrc2=size=720x1280 -frames:v 1 -c:v libx264 -profile:v high -f h264 build/tests/portrait.h264
ffmpeg -hide_banner -loglevel error -y -f lavfi -i sine=sample_rate=44100 -t 1 -ac 2 -c:a aac -f adts build/tests/audio.aac
ffmpeg_cflags="$(pkg-config --cflags libavcodec libswresample libavutil 2>/dev/null || true)"
ffmpeg_libs="$(pkg-config --libs libavcodec libswresample libavutil 2>/dev/null || true)"
if [ -z "$ffmpeg_libs" ] && command -v brew >/dev/null; then
  ffmpeg_root="$(brew --prefix ffmpeg)"
  ffmpeg_cflags="-I$ffmpeg_root/include"
  ffmpeg_libs="-L$ffmpeg_root/lib -lavcodec -lswresample -lavutil"
fi
if [ -z "$ffmpeg_libs" ]; then
  echo 'FFmpeg development files are required (pkg-config or Homebrew ffmpeg).' >&2
  exit 2
fi
c++ -std=c++11 -g -fsanitize=address,undefined -Isrc $ffmpeg_cflags \
  tests/media_test.cpp src/h264.cpp src/audio.cpp \
  $ffmpeg_libs -o build/tests/media-test
build/tests/media-test build/tests/landscape.h264 build/tests/portrait.h264 build/tests/audio.aac
c++ -std=c++11 -g -fsanitize=address,undefined -Isrc tests/hls_playlist_test.cpp -o build/tests/hls-playlist-test
build/tests/hls-playlist-test
c++ -std=c++11 -g -fsanitize=address,undefined -Isrc tests/hls_bridge_driver.cpp src/hls_bridge.cpp -o build/tests/hls-bridge-test
python3 tests/hls_bridge_test.py build/tests/hls-bridge-test
python3 tests/companion_test.py
