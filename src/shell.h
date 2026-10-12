#pragma once
#include <SDL.h>
#include <cstdint>
#include <string>
struct _TTF_Font;
enum class Action { None, Back, Exit, Left, Right };
class Shell {
public:
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    std::string title="UxPlay webOS";
    bool open();
    bool pump();
    Action action();
    void screen(const std::string& status, const std::string& pin, int audio_delay_ms=100);
    void transparent();
    ~Shell();
private:
    _TTF_Font *font_=nullptr, *large_=nullptr;
    void text(const std::string&,int y,_TTF_Font*,SDL_Color);
};
int64_t now_ns();
std::string app_directory();
void start_log(const char *name);
void rotate_log();
