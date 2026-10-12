#include "hls_playlist.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
 const char *source="#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://localhost:1234/audio.m3u8\"\n"
 "#EXT-X-STREAM-INF:CODECS=\"vp09.00.31.08,mp4a.40.2\",RESOLUTION=1280x720\n/vp9.m3u8\n"
 "#EXT-X-STREAM-INF:CODECS=\"avc1.640028,mp4a.40.2\",RESOLUTION=1920x1080\n/1080.m3u8\n"
 "#EXT-X-STREAM-INF:CODECS=\"avc1.4D401F,mp4a.40.2\",RESOLUTION=1280x720\nhttp://localhost:1234/720.m3u8\n";
 char *result=webos_hls_master(source);assert(result);webos_hls_loopback(result);
 assert(!strstr(result,"vp09"));assert(!strstr(result,"1080.m3u8"));assert(!strstr(result,"localhost"));
 assert(strstr(result,"127.0.0.1:1234/audio.m3u8"));assert(strstr(result,"127.0.0.1:1234/720.m3u8"));free(result);
 result=webos_hls_master("#EXTM3U\n#EXT-X-STREAM-INF:CODECS=\"unknown\"\nunsupported");assert(result&&!strstr(result,"unsupported"));free(result);
 result=webos_hls_master("");assert(result&&!*result);free(result);
 puts("HLS playlist compatibility: PASS");
}
