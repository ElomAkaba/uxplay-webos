#include "hls_bridge.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

std::string merged_hls_url(const std::string& original, const std::string& config_path) {
    std::ifstream config(config_path);
    if (!config) return original;
    std::string host, service, token;
    std::getline(config, host); std::getline(config, service); std::getline(config, token);
    if (host.empty() || service.empty() || service.size()>5 || service.find_first_not_of("0123456789")!=std::string::npos ||
        token.size()!=64 || token.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos) return "";
    long bridge_port=std::strtol(service.c_str(),nullptr,10);
    if (bridge_port<1024 || bridge_port>65535 || original.compare(0,17,"http://localhost:")!=0) return "";
    const size_t slash=original.find('/',17);
    if (slash==std::string::npos || original.substr(slash)!="/master.m3u8") return "";
    const std::string port=original.substr(17,slash-17);
    if (port.empty() || port.size()>5 || port.find_first_not_of("0123456789")!=std::string::npos) return "";
    long source_port=std::strtol(port.c_str(),nullptr,10);
    if (source_port<1024 || source_port>65535) return "";
    sockaddr_in address={};address.sin_family=AF_INET;address.sin_port=htons(bridge_port);
    if(inet_pton(AF_INET,host.c_str(),&address.sin_addr)!=1)return "";
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return "";
    int flags=fcntl(fd,F_GETFL,0);
    if(flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){close(fd);return "";}
    if(connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0) {
        pollfd wait={fd,POLLOUT,0};int error=0;socklen_t size=sizeof(error);
        if(errno!=EINPROGRESS || poll(&wait,1,5000)<=0 || getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size)<0 || error){close(fd);return "";}
    }
    fcntl(fd,F_SETFL,flags);
    timeval timeout={35,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    const std::string base="http://"+host+":"+service+"/";
    std::string request="GET /start?port="+port+" HTTP/1.1\r\nHost: "+host+":"+service+"\r\nAuthorization: Bearer "+token+"\r\nConnection: close\r\n\r\n";
    size_t sent=0;
    while(sent<request.size()) {ssize_t n=send(fd,request.data()+sent,request.size()-sent,MSG_NOSIGNAL);if(n<=0){close(fd);return "";}sent+=n;}
    std::string reply;char buffer[2048];ssize_t n;
    while((n=recv(fd,buffer,sizeof(buffer),0))>0) {reply.append(buffer,n);if(reply.size()>8192){close(fd);return "";}}
    close(fd);
    if(n<0 || (reply.compare(0,12,"HTTP/1.0 200") && reply.compare(0,12,"HTTP/1.1 200")))return "";
    const size_t body=reply.find("\r\n\r\n");if(body==std::string::npos)return "";
    std::string url=reply.substr(body+4);
    if(url.compare(0,base.size(),base)!=0 || url.find_first_of("\r\n\t ")!=std::string::npos)return "";
    fprintf(stderr,"[hls] audio bridge prepared combined stream\n");return url;
}
