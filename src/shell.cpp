#include "shell.h"
#include <SDL_webOS.h>
#include <SDL_ttf.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
static std::string log_path;
static volatile sig_atomic_t interrupted = 0;
static void signal_stop(int) { interrupted = 1; }
int64_t now_ns() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return int64_t(t.tv_sec)*1000000000 + t.tv_nsec; }
std::string app_directory() {
    char p[4096]; ssize_t n = readlink("/proc/self/exe", p, sizeof(p)-1);
    if (n <= 0) return ".";
    p[n] = 0; std::string s(p); return s.substr(0, s.rfind('/'));
}
void start_log(const char *name) {
    std::string p = app_directory() + "/" + name;
    log_path=p;
    rename(p.c_str(), (p+".previous").c_str());
    freopen(p.c_str(), "w", stderr);
    setvbuf(stderr, nullptr, _IOLBF, 0);
    // Proprietary drivers emit high-volume frame diagnostics to stdout.
    // Keep application/errors on stderr, without filling app storage with them.
    int nullfd=open("/dev/null",O_WRONLY);
    if(nullfd>=0){dup2(nullfd,STDOUT_FILENO);close(nullfd);}
}
void rotate_log() {
    struct stat s;
    if(!log_path.empty() && fstat(fileno(stderr),&s)==0 && s.st_size>1024*1024) {
        rename(log_path.c_str(),(log_path+".previous").c_str());
        freopen(log_path.c_str(),"w",stderr);setvbuf(stderr,nullptr,_IOLBF,0);
    }
}
bool Shell::open() {
    signal(SIGTERM, signal_stop); signal(SIGINT, signal_stop);
    setenv("EGL_PLATFORM", "wayland", 0); setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 0);
    SDL_SetHint(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK, "true");
    SDL_SetHint(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT, "true");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "[SDL] %s\n",SDL_GetError()); return false; }
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0,&mode) != 0) return false;
    window = SDL_CreateWindow(title.c_str(),0,0,mode.w,mode.h,SDL_WINDOW_FULLSCREEN);
    if (window) renderer = SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
    if (!renderer) { fprintf(stderr,"[SDL] %s\n",SDL_GetError()); return false; }
    fprintf(stderr,"[SDL] window %dx%d\n",mode.w,mode.h);
    if(TTF_Init()!=0) return false;
    font_=TTF_OpenFont("/usr/share/fonts/LG_Display-Regular.ttf",30);
    large_=TTF_OpenFont("/usr/share/fonts/LG_Display-Regular.ttf",76);
    if(!font_||!large_) {fprintf(stderr,"[font] %s\n",TTF_GetError());return false;}
    return true;
}
void Shell::transparent() {
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer,0,0,0,0); SDL_RenderClear(renderer); SDL_RenderPresent(renderer);
}
Action Shell::action() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return Action::Exit;
        if (e.type == SDL_KEYDOWN) {
            if(e.key.keysym.scancode==SDL_SCANCODE_LEFT)return Action::Left;
            if(e.key.keysym.scancode==SDL_SCANCODE_RIGHT)return Action::Right;
            if(e.key.keysym.scancode==SDL_WEBOS_SCANCODE_EXIT) return Action::Exit;
            if(e.key.keysym.scancode==SDL_WEBOS_SCANCODE_BACK || e.key.keysym.scancode==SDL_SCANCODE_ESCAPE || e.key.keysym.scancode==SDL_SCANCODE_AC_BACK) return Action::Back;
        }
    }
    return interrupted?Action::Exit:Action::None;
}
bool Shell::pump() {Action a=action();return a!=Action::Back&&a!=Action::Exit;}
void Shell::text(const std::string& s,int y,TTF_Font *font,SDL_Color color) {
    SDL_Surface *surface=TTF_RenderUTF8_Blended(font,s.c_str(),color);if(!surface)return;
    SDL_Texture *texture=SDL_CreateTextureFromSurface(renderer,surface);
    int w,h;SDL_GetRendererOutputSize(renderer,&w,&h);
    int width=std::min(surface->w,w-80),height=surface->h;
    SDL_Rect rect={(w-width)/2,y,width,height};
    if(texture) {SDL_RenderCopy(renderer,texture,nullptr,&rect);SDL_DestroyTexture(texture);}
    SDL_FreeSurface(surface);
}
void Shell::screen(const std::string& status,const std::string& pin,int delay) {
    SDL_SetRenderDrawColor(renderer,14,25,36,255);SDL_RenderClear(renderer);
    int w,h;SDL_GetRendererOutputSize(renderer,&w,&h);
    text(title,h/6,large_,{65,210,171,255});
    text(status,h/2-60,font_,{238,242,246,255});
    if(!pin.empty()) text(pin,h/2+10,large_,{255,255,255,255});
    else text("Select "+title+" on your iPhone, iPad or Mac",h/2+20,font_,{178,194,207,255});
    char sync[160];snprintf(sync,sizeof(sync),"◀ ▶  Audio delay: %+d ms  ·  Adjust before connecting",delay);
    text(sync,h-135,font_,{145,167,185,255});
    text("Same Wi-Fi network  ·  720p / 30 fps  ·  Back to exit",h-80,font_,{145,167,185,255});
    SDL_RenderPresent(renderer);
}
Shell::~Shell() {if(font_)TTF_CloseFont(font_);if(large_)TTF_CloseFont(large_);TTF_Quit();if(renderer) SDL_DestroyRenderer(renderer); if(window) SDL_DestroyWindow(window); SDL_Quit(); }
