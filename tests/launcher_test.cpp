#include "launcher.h"
#include <cassert>
#include <fstream>
#include <cstdio>
#include <unistd.h>
int main(){
 char path[]="/tmp/uxplay-launcher-test-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
 {std::ofstream out(path);out<<"com.example.launcher\nLiving Room\n";}
 auto config=read_launcher_integration(path);assert(config.enabled());assert(config.app_id=="com.example.launcher");assert(config.receiver_name=="Living Room");
 {std::ofstream out(path);out<<"com.example.launcher\n\n";}
 config=read_launcher_integration(path);assert(config.receiver_name=="Lumen TV");
 {std::ofstream out(path);out<<"com.example.launcher';touch /tmp/injected\nRoom\n";}
 assert(!read_launcher_integration(path).enabled());assert(!launch_application("bad\"id"));
 unlink(path);assert(!read_launcher_integration(path).enabled());
 puts("Launcher configuration and application ID validation: PASS");
}
