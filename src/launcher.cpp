#include "launcher.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <cerrno>

static bool valid_id(const std::string& id) {
    return !id.empty() && id.size()<=128 && id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_")==std::string::npos;
}
LauncherIntegration read_launcher_integration(const std::string& path) {
    LauncherIntegration config;
    std::ifstream input(path);std::getline(input,config.app_id);std::getline(input,config.receiver_name);
    if(!valid_id(config.app_id)) return {};
    if(config.receiver_name.empty() || config.receiver_name.size()>60 || config.receiver_name.find_first_of("\r\n\t")!=std::string::npos) config.receiver_name="Lumen TV";
    return config;
}
bool launch_application(const std::string& app_id) {
    if(!valid_id(app_id))return false;
    int input[2],output[2];
    if(pipe(input)<0)return false;
    if(pipe(output)<0){close(input[0]);close(input[1]);return false;}
    const std::string payload="{\"id\":\""+app_id+"\"}";
    pid_t child=fork();
    if(child<0){close(input[0]);close(input[1]);close(output[0]);close(output[1]);return false;}
    if(!child) {
        dup2(input[0],STDIN_FILENO);dup2(output[1],STDOUT_FILENO);
        close(input[0]);close(input[1]);close(output[0]);close(output[1]);
        execl("/usr/bin/luna-send-pub","luna-send-pub","-n","1","luna://com.webos.applicationManager/launch",payload.c_str(),static_cast<char*>(nullptr));
        _exit(127);
    }
    close(input[0]);close(output[1]);
    // Keep stdin open until the response arrives: older luna-send exits on EOF.
    pollfd wait={output[0],POLLIN,0};char buffer[1024];std::string reply;
    timespec started;clock_gettime(CLOCK_MONOTONIC,&started);
    while(reply.size()<4096) {
        timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
        long remaining=4000-(now.tv_sec-started.tv_sec)*1000-(now.tv_nsec-started.tv_nsec)/1000000;
        if(remaining<=0)break;
        int result=poll(&wait,1,remaining);
        if(result<0 && errno==EINTR)continue;
        if(result<=0)break;
        ssize_t count=read(output[0],buffer,sizeof(buffer));
        if(count<0 && errno==EINTR)continue;
        if(count<=0)break;
        reply.append(buffer,count);
        if(reply.find('}')!=std::string::npos)break;
    }
    close(output[0]);close(input[1]);
    int status=0;
    if(waitpid(child,&status,WNOHANG)==0){kill(child,SIGTERM);waitpid(child,&status,0);}
    bool accepted=reply.find("\"returnValue\":true")!=std::string::npos||reply.find("\"returnValue\": true")!=std::string::npos;
    if(!accepted)fprintf(stderr,"[launcher] return to home failed\n");
    return accepted;
}
