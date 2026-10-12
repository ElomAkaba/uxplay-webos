#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <string>
class StarfishMediaAPIs;
enum class FeedResult { Accepted, Retry, Error };
class Media {
public:
    Media();
    ~Media();
    bool open(const char *app_id, int width, int height, bool audio, int sample_rate = 44100);
    bool open_url(const char *app_id, const std::string& url);
    void seek(double seconds);
    std::atomic<double> duration{0};
    FeedResult feed(const void *data, size_t size, int64_t pts_ns, bool video);
    void tick();
    void eos();
    void pause(bool paused);
    void close();
    std::atomic<bool> ready{false}, playing{false}, ended{false}, failed{false};
    std::atomic<unsigned> frames{0};
private:
    static void event(int type, int64_t value, const char *message, void *user);
    StarfishMediaAPIs *api_ = nullptr;
    long acb_ = 0, task_ = 0;
    bool loaded_ = false, sink_attached_ = false, play_requested_ = false, acb_playing_ = false;
    std::atomic<bool> video_info_pending_{false};
    // Firmware callbacks and Feed run on separate threads.
    struct Impl;
    Impl *impl_;
};
