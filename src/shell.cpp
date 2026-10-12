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
#include <cmath>
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
    SDL_RenderSetLogicalSize(renderer,1920,1080);
    SDL_Surface *background=SDL_LoadBMP((app_directory()+"/state/wallpaper.bmp").c_str());
    if(background){wallpaper_=SDL_CreateTextureFromSurface(renderer,background);SDL_FreeSurface(background);}
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
static void rounded_panel(SDL_Renderer *renderer,int x,int y,int w,int h,int radius,SDL_Color color) {
    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);
    for(int row=0;row<h;++row){
        int edge=0;
        if(row<radius){double d=radius-row-0.5;edge=radius-int(std::sqrt(radius*radius-d*d));}
        else if(row>=h-radius){double d=row-(h-radius)+0.5;edge=radius-int(std::sqrt(radius*radius-d*d));}
        SDL_RenderDrawLine(renderer,x+edge,y+row,x+w-edge-1,y+row);
    }
}
void Shell::screen(const std::string& status,const std::string& pin,int delay) {
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer,20,25,34,255);SDL_RenderClear(renderer);
    if(wallpaper_)SDL_RenderCopy(renderer,wallpaper_,nullptr,nullptr);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,0,0,0,95);SDL_Rect shade={0,0,1920,1080};SDL_RenderFillRect(renderer,&shade);
    rounded_panel(renderer,430,170,1060,740,42,{185,194,208,90});
    rounded_panel(renderer,432,172,1056,736,40,{23,29,40,232});
    // AirPlay outline, using the same screen-and-triangle shape as the launcher.
    SDL_SetRenderDrawColor(renderer,255,255,255,255);
    for(int i=0;i<4;++i){
        SDL_RenderDrawLine(renderer,919+i,235+i,1001-i,235+i);
        SDL_RenderDrawLine(renderer,919+i,235,919+i,286);
        SDL_RenderDrawLine(renderer,1001-i,235,1001-i,286);
        SDL_RenderDrawLine(renderer,919,286-i,938,286-i);
        SDL_RenderDrawLine(renderer,982,286-i,1001,286-i);
        SDL_RenderDrawLine(renderer,960,273-i,932-i,304+i);
        SDL_RenderDrawLine(renderer,960,273-i,988+i,304+i);
        SDL_RenderDrawLine(renderer,932,304+i,988,304+i);
    }
    text(title,330,large_,{255,255,255,255});
    std::string message=status;
    if(status=="Waiting for a connection")message="Klaar om te verbinden";
    if(status=="Starting AirPlay receiver")message="AirPlay starten…";
    text(message,465,font_,{238,242,246,255});
    if(!pin.empty()) {
        text("Vul deze code in op je apparaat",535,font_,{194,200,210,255});
        text(pin,590,large_,{255,255,255,255});
    } else {
        text("Kies "+title+" op je iPhone, iPad of Mac",545,font_,{238,242,246,255});
        text("Open AirPlay of Synchrone weergave om te beginnen",605,font_,{194,200,210,255});
    }
    char sync[160];snprintf(sync,sizeof(sync),"◀  ▶   Audiovertraging: %+d ms",delay);
    text(sync,745,font_,{194,200,210,255});
    text("Zelfde wifi-netwerk  ·  Druk op Terug voor het beginscherm",815,font_,{194,200,210,255});
    SDL_RenderPresent(renderer);
}
Shell::~Shell() {if(wallpaper_)SDL_DestroyTexture(wallpaper_);if(font_)TTF_CloseFont(font_);if(large_)TTF_CloseFont(large_);TTF_Quit();if(renderer) SDL_DestroyRenderer(renderer); if(window) SDL_DestroyWindow(window); SDL_Quit(); }
