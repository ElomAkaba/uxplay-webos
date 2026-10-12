#include "audio.h"
#include "h264.h"
#include "media.h"
#include "receiver.h"
#include "shell.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <sys/stat.h>

int main() {
    umask(0077);start_log("uxplay.log");
    Shell shell;if(!shell.open())return 1;
    shell.screen("Starting AirPlay receiver","");
    Receiver receiver;
    if(!receiver.start(app_directory()+"/state")) {
        shell.screen("Could not start. See uxplay.log","");
        while(shell.pump())SDL_Delay(50);
        return 1;
    }
    const char *app_id=getenv("APPID");if(!app_id)app_id="com.github.danlflh.uxplaywebos";
    Media media;AudioDecoder decoder;
    bool opened=false,have_pending=false,last_paused=false,audio_started=false;
    int width=1280,height=720;
    Packet pending;
    std::vector<uint8_t> configuration;
    uint64_t origin=0;
    int64_t audio_end=0;
    std::vector<uint8_t> silence(441*4,0);
    int64_t start=0,last_ui=0,last_stats=0,stall=0;
    unsigned video_count=0,audio_count=0,decode_errors=0,late_audio=0,silence_blocks=0;
    auto finish=[&](){
        media.close();decoder.reset();receiver.disconnect();opened=false;have_pending=false;
        origin=0;start=0;stall=0;audio_end=0;configuration.clear();last_ui=0;last_paused=false;audio_started=false;
    };
    bool hls_open=false, hls_paused=false; double hls_position=0; int64_t hls_tick=0;
    bool running=true;
    while(running) {
        int64_t now=now_ns();
        Action action=shell.action();
        if(action==Action::Exit)break;
        if(action==Action::Back) {
            if(opened||receiver.mirror||!receiver.pin().empty())finish();else break;
        }
        if(!opened&&(action==Action::Left||action==Action::Right)) {
            receiver.adjust_audio_delay(action==Action::Right?25:-25);last_ui=0;
        }
        if(receiver.hls_requested.exchange(false)) {
            media.close(); opened=false; have_pending=false; receiver.clear(); receiver.reset=false;
            std::string url; {std::lock_guard<std::mutex> l(receiver.mutex);url=receiver.hls_url;}
            hls_open=media.open_url(app_id,url); hls_position=0; hls_tick=now; hls_paused=false;
            receiver.hls_ready=false; receiver.hls_position=0;
            if(hls_open) shell.transparent(); else {receiver.hls_active=false;receiver.show("YouTube playback failed. See log.");}
        }
        if(hls_open) {
            media.tick();
            bool paused=receiver.paused; if(paused!=hls_paused) {media.pause(paused);hls_paused=paused;}
            double seek=media.ready?receiver.hls_seek.exchange(-1):-1; if(seek>=0) {media.seek(seek);hls_position=seek;}
            if(media.playing && !paused) hls_position+=double(now-hls_tick)/1000000000.0;
            hls_tick=now; receiver.hls_position=hls_position; receiver.hls_duration=media.duration.load(); receiver.hls_ready=media.ready.load();
            if(action==Action::Back || receiver.hls_stop.exchange(false) || media.failed || media.ended) {
                fprintf(stderr,"[hls] playback ended failed=%d\n",int(media.failed.load()));
                media.close();hls_open=false;receiver.hls_active=false;receiver.hls_ready=false;
                receiver.show("Waiting for a connection"); last_ui=0;
            }
            if(hls_open) {SDL_Delay(10);continue;}
        }
        if(receiver.reset.exchange(false))finish();
        if(receiver.activity && now-receiver.activity>20000000000LL && (opened||receiver.mirror)) {
            fprintf(stderr,"[session] idle timeout\n");
            finish();receiver.show("Connection lost. Connect again.");
        }
        if(receiver.audio_flush.exchange(false))decoder.reset();
        if(opened) {
            media.tick();
            bool paused=receiver.paused;
            if(paused!=last_paused) {media.pause(paused);last_paused=paused;}
            if(media.failed) {finish();receiver.show("Playback error. Connect again.");}
            // Some senders negotiate audio after mirroring starts. Prime the PCM
            // track only until the first real packet arrives. Filling later gaps
            // advances audio_end past delayed RTP packets and drops real audio.
            int fills=0;
            while(opened && !audio_started && !last_paused && now-receiver.last_audio_arrival>500000000 && now-start-audio_end>100000000 && fills++<10) {
                auto r=media.feed(silence.data(),silence.size(),audio_end,false);
                if(r!=FeedResult::Accepted)break;
                audio_end+=10000000;
                ++silence_blocks;
            }
        }
        if(!opened && now-last_ui>200000000) {
            shell.screen(receiver.status(),receiver.pin(),receiver.audio_delay_ms);last_ui=now;
        }
        if(now-last_stats>5000000000LL) {
            if(opened)fprintf(stderr,"[stats] video=%u audio=%u frame_events=%u overflow=%u audio_errors=%u late_audio=%u silence=%u\n",video_count,audio_count,media.frames.load(),receiver.overflow.load(),decode_errors,late_audio,silence_blocks);
            rotate_log();last_stats=now;
        }
        if(!have_pending) {
            // Leave future packets in the shared queue so late-arriving video can
            // precede deliberately delayed audio. Avoid head-of-line blocking.
            uint64_t latest=opened?origin+uint64_t(std::max<int64_t>(0,now-start)):UINT64_MAX;
            have_pending=receiver.pop(pending,latest);
        }
        if(!have_pending){SDL_Delay(3);continue;}
        if(!opened) {
            // Start on a random-access video frame. Pre-roll audio is intentionally
            // discarded, then both streams share this frame's remote NTP origin.
            if(!pending.video) {have_pending=false;continue;}
            auto info=inspect_h264(pending.data.data(),pending.data.size());
            if(!info.valid){have_pending=false;continue;}
            if(!info.configuration.empty())configuration=info.configuration;
            if(info.width) {width=info.width;height=info.height;}
            if(!info.keyframe||configuration.empty()) {have_pending=false;continue;}
            if(!media.open(app_id,width,height,true)) {
                finish();receiver.show("Could not open the TV decoder.");continue;
            }
            opened=true;origin=pending.timestamp;start=now+200000000;stall=0;
            video_count=audio_count=decode_errors=late_audio=silence_blocks=0;
            shell.transparent();
            fprintf(stderr,"[session] %dx%d audio=%d\n",width,height,int(receiver.has_audio));
        }
        int64_t pts=pending.timestamp>=origin ? int64_t(pending.timestamp-origin) : -int64_t(origin-pending.timestamp);
        if(pts<0){have_pending=false;continue;}
        // Large clock discontinuities require a fresh AirPlay session, not an unbounded queue.
        if(pts>now-start+5000000000LL) {fprintf(stderr,"[session] timestamp discontinuity\n");finish();receiver.show("Device clock changed. Connect again.");continue;}
        if(now<start+pts) {SDL_Delay(2);continue;}
        if(pending.video && pending.codec==0) {
            auto info=inspect_h264(pending.data.data(),pending.data.size());
            if(!info.valid){have_pending=false;continue;}
            if(!info.configuration.empty())configuration=info.configuration;
            if(info.width && (width!=info.width||height!=info.height)) {
                media.close();width=info.width;height=info.height;
                if(!info.keyframe || !media.open(app_id,width,height,true)) {finish();continue;}
                origin=pending.timestamp;start=now;pts=0;audio_end=0;audio_started=false;decoder.reset();
                fprintf(stderr,"[session] geometry=%dx%d\n",width,height);
            }
            pending.codec=1; // Already parsed: do not repeat on decoder back-pressure.
        }
        if(!pending.video && pending.codec!=0) {
            std::vector<uint8_t> pcm;
            if(!decoder.decode(pending.codec,pending.data.data(),pending.data.size(),pcm)) {
                if(++decode_errors<=5)fprintf(stderr,"[audio] decode failure ct=%u\n",pending.codec);
                have_pending=false;continue;
            }
            pending.data.swap(pcm);pending.codec=0;
            float volume=receiver.volume;
            if(volume<1) {
                for(size_t i=0;i+1<pending.data.size();i+=2) {
                    int16_t sample=int16_t(unsigned(pending.data[i])|(unsigned(pending.data[i+1])<<8));
                    int16_t out=int16_t(sample*volume);pending.data[i]=out&255;pending.data[i+1]=(uint16_t(out)>>8)&255;
                }
            }
            if(pending.data.empty()){have_pending=false;continue;}
        }
        if(!pending.video) audio_started=true;
        if(!pending.video && pts<audio_end) {
            size_t trim=size_t((audio_end-pts)*44100/1000000000)*4;
            if(trim>=pending.data.size()){++late_audio;have_pending=false;continue;}
            pending.data.erase(pending.data.begin(),pending.data.begin()+trim);
            pending.timestamp=origin+audio_end;pts=audio_end;
        }
        auto result=media.feed(pending.data.data(),pending.data.size(),pts,pending.video);
        if(result==FeedResult::Accepted) {
            if(pending.video)++video_count;else {++audio_count;audio_end=pts+int64_t(pending.data.size()/4)*1000000000/44100;}
            have_pending=false;stall=0;
        } else if(result==FeedResult::Retry) {
            if(!stall)stall=now;
            if(now-stall>3000000000LL){finish();receiver.show("Playback stopped. Connect again.");}
            SDL_Delay(5);
        } else {finish();receiver.show("Stream failure. Connect again.");}
    }
    receiver.stop();media.close();
    fprintf(stderr,"[app] stopped cleanly\n");
    return 0;
}
