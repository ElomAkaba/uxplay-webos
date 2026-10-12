#include "media.h"
#include <StarfishMediaAPIs.h>
#include <AcbAPI.h>
#include <cstdio>
#include <mutex>
#include <sstream>
#include "hls_bridge.h"
#include "shell.h"
// This firmware exports a three-argument wrapper; the NDK declaration omits taskId.
extern "C" int acb_video_info(long, const char *, long *) __asm__("AcbAPI_setMediaVideoData");
struct Media::Impl { std::mutex mutex; std::string video_info; };
Media::Media() : impl_(new Impl) {}
Media::~Media() { close(); delete impl_; }
static void acb_event(long, long, long event, long app, long play, const char *reply) {
    fprintf(stderr, "[acb] event=%ld app=%ld play=%ld %s\n", event, app, play, reply ? reply : "");
}
void Media::event(int type, int64_t value, const char *message, void *user) {
    Media *m = static_cast<Media *>(user);
    if (type == PF_EVENT_TYPE_FRAMEREADY) { ++m->frames; return; }
    if (type == PF_EVENT_TYPE_INT_NEED_DATA || type == PF_EVENT_TYPE_INT_ENOUGH_DATA || type == PF_EVENT_TYPE_INT_BUFFERLOW || type == 0x2a) return;
    fprintf(stderr, "[smp] event=0x%x value=%lld %s\n", type, (long long)value, message ? message : "");
    switch (type) {
    case PF_EVENT_TYPE_INT_DURATION: m->duration = value/1000.0; break;
    case PF_EVENT_TYPE_STR_STATE_UPDATE__LOADCOMPLETED: m->ready = true; break;
    case PF_EVENT_TYPE_STR_STATE_UPDATE__PLAYING: m->playing = true; break;
    case PF_EVENT_TYPE_STR_STATE_UPDATE__ENDOFSTREAM: m->ended = true; break;
    case PF_EVENT_TYPE_INT_ERROR:
    case PF_EVENT_TYPE_STR_ERROR: m->failed = true; break;
    case PF_EVENT_TYPE_STR_VIDEO_INFO:
        if (message) {
            std::lock_guard<std::mutex> lock(m->impl_->mutex);
            m->impl_->video_info = message;
            m->video_info_pending_ = true;
        }
        break;
    default: break;
    }
}
bool Media::open(const char *app_id, int width, int height, bool audio, int rate) {
    close();
    ready = playing = ended = failed = false;
    frames = 0;
    acb_ = AcbAPI_create();
    if (!acb_ || !AcbAPI_initialize(acb_, PLAYER_TYPE_MSE, app_id, acb_event)) {
        fprintf(stderr, "[media] ACB initialization failed\n"); close(); return false;
    }
    api_ = new StarfishMediaAPIs(nullptr);
    if (!AcbAPI_setMediaId(acb_, api_->getMediaID())) {
        fprintf(stderr, "[media] ACB media registration failed\n"); close(); return false;
    }
    std::ostringstream p;
    p << "{\"args\":[{\"mediaTransportType\":\"BUFFERSTREAM\",\"option\":{\"appId\":\"" << app_id
      << "\",\"queryPosition\":false,\"externalStreamingInfo\":{\"contents\":{\"codec\":{\"video\":\"H264\"";
    if (audio) p << ",\"audio\":\"PCM\"";
    p << "},\"esInfo\":{\"pauseAtDecodeTime\":true,\"ptsToDecode\":0,\"seperatedPTS\":true,\"videoFpsValue\":30,\"videoFpsScale\":1},\"format\":\"RAW\",\"provider\":\"Chrome\"";
    if (audio) p << ",\"pcmInfo\":{\"channelMode\":\"stereo\",\"format\":\"S16LE\",\"sampleRate\":" << rate / 1000.0
                 << ",\"bitsPerSample\":16,\"layout\":\"interleaved\"}";
    p << "},\"restartStreaming\":false,\"streamQualityInfo\":true,\"streamQualityInfoNonFlushable\":true,\"totalStreamSize\":256,"
         "\"bufferingCtrInfo\":{\"preBufferByte\":0,\"bufferMinLevel\":0,\"bufferMaxLevel\":0,\"qBufferLevelAudio\":0,\"qBufferLevelVideo\":0,"
         "\"srcBufferLevelAudio\":{\"minimum\":1024,\"maximum\":131072},\"srcBufferLevelVideo\":{\"minimum\":1024,\"maximum\":1048576}}},"
         "\"adaptiveStreaming\":{\"audioOnly\":false,\"maxWidth\":" << width << ",\"maxHeight\":" << height << ",\"maxFrameRate\":30}}}]}";
    fprintf(stderr, "[media] Load %s\n", p.str().c_str());
    loaded_ = api_->Load(p.str().c_str(), event, this);
    if (!loaded_) { close(); return false; }
    AcbAPI_setDisplayWindow(acb_, 0, 0, width, height, true, &task_);
    return true;
}
bool Media::open_url(const char *app_id, const std::string& url) {
    close(); ready=playing=ended=failed=false; duration=0; frames=0;
    acb_=AcbAPI_create();
    if(!acb_ || !AcbAPI_initialize(acb_,PLAYER_TYPE_MSE,app_id,acb_event)) {close();return false;}
    api_=new StarfishMediaAPIs(nullptr);
    if(!AcbAPI_setMediaId(acb_,api_->getMediaID())) {close();return false;}
    // The URL is supplied by UxPlay's local HLS proxy, never by a shell command.
    std::string playback_url=merged_hls_url(url, app_directory()+"/state/hls-bridge.conf");
    if(playback_url.empty()){fprintf(stderr,"[hls] audio bridge preparation failed\n");close();return false;}
    std::string escaped; for(char c:playback_url) {if(c=='"'||c=='\\')escaped+='\\';escaped+=c;}
    // Avoid localhost resolving to IPv6 on older streaming engines.
    if (escaped.compare(0,17,"http://localhost:")==0) escaped.replace(0,17,"http://127.0.0.1:");
    // webOS 3's generateJsonPayloadForPlayer treats args[0] as the URI
    // string and args[1] as load options. HLS selects the streaming player.
    std::string payload="{\"args\":[\""+escaped+"\",{\"mediaTransportType\":\"HLS\",\"option\":{\"appId\":\""+app_id+"\",\"adaptiveStreaming\":{\"maxWidth\":1280,\"maxHeight\":720,\"maxFrameRate\":30}}}]}";
    fprintf(stderr,"[hls] loading native HLS player\n");
    try {
        loaded_=api_->Load(payload.c_str(),event,this);
    } catch (const std::exception& error) {
        fprintf(stderr,"[hls] native player load failed: %s\n",error.what());
        failed=true; close(); return false;
    }
    if(!loaded_) {close();return false;}
    AcbAPI_setDisplayWindow(acb_,0,0,1920,1080,true,&task_);
    return true;
}
void Media::seek(double seconds) {
    if(api_ && loaded_ && seconds>=0) {std::string ms=std::to_string((long long)(seconds*1000)); api_->Seek(ms.c_str());}
}
void Media::tick() {
    if (!api_ || !loaded_ || failed) return;
    if (ready && !sink_attached_) {
        AcbAPI_setSinkType(acb_, SINK_TYPE_MAIN);
        AcbAPI_setState(acb_, APPSTATE_FOREGROUND, PLAYSTATE_LOADED, &task_);
        sink_attached_ = true;
    }
    if (video_info_pending_.exchange(false)) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        acb_video_info(acb_, impl_->video_info.c_str(), &task_);
    }
    if (ready && !play_requested_) play_requested_ = api_->Play();
    if (playing && sink_attached_ && !acb_playing_) {
        AcbAPI_setState(acb_, APPSTATE_FOREGROUND, PLAYSTATE_PLAYING, &task_);
        // ACB only needs the transition once; playing remains observable by the caller.
        acb_playing_=true;
    }
}
FeedResult Media::feed(const void *data, size_t size, int64_t pts_ns, bool video) {
    if (!api_ || !loaded_ || failed || !data || !size) return FeedResult::Error;
    char payload[200];
    snprintf(payload, sizeof(payload), "{\"bufferAddr\":\"%p\",\"bufferSize\":%zu,\"pts\":%lld,\"esData\":%d}",
             data, size, (long long)pts_ns, video ? 1 : 2);
    std::string reply = api_->Feed(payload);
    if (reply.find("Ok") != std::string::npos) return FeedResult::Accepted;
    if (reply.find("BufferFull") != std::string::npos || reply.find("Pending") != std::string::npos) return FeedResult::Retry;
    fprintf(stderr, "[media] Feed rejected: %s\n", reply.c_str());
    return FeedResult::Error;
}
void Media::eos() { if (api_ && loaded_) api_->pushEOS(); }
void Media::pause(bool paused) {if(api_&&loaded_) {if(paused)api_->Pause();else api_->Play();}}
void Media::close() {
    if(acb_ && loaded_) AcbAPI_setState(acb_,APPSTATE_FOREGROUND,PLAYSTATE_UNLOADED,&task_);
    if (api_) {
        if (loaded_) { api_->Unload(); fprintf(stderr, "[media] unloaded\n"); }
        delete api_; api_ = nullptr;
    }
    if (acb_) {
        AcbAPI_finalize(acb_); AcbAPI_destroy(acb_); acb_ = 0;
        fprintf(stderr, "[media] ACB released\n");
    }
    loaded_ = sink_attached_ = play_requested_ = acb_playing_ = false;
    video_info_pending_ = false;
}
