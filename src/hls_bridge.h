#pragma once
#include <string>
// Missing configuration leaves direct HLS playback enabled. A configured bridge
// returns an empty string on failure, so missing audio is not silently accepted.
std::string merged_hls_url(const std::string& original, const std::string& config_path);
