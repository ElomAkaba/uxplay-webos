#ifndef WEBOS_HLS_COMPAT_H
#define WEBOS_HLS_COMPAT_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static char *webos_hls_master(const char *source) {
    size_t size=strlen(source); char *out=(char *)malloc(size+1); if(!out) return NULL;
    const char *line=source; char *write=out; int skip_uri=0;
    while(*line) {
        const char *end=strchr(line,'\n'); size_t len=end?(size_t)(end-line)+1:strlen(line);
        int keep=1;
        if(len>=18 && !strncmp(line,"#EXT-X-STREAM-INF:",18)) {
            char *header=(char *)malloc(len+1); if(!header){free(out);return NULL;}
            memcpy(header,line,len);header[len]=0;
            const char *codec=strstr(header,"CODECS=\""); const char *resolution=strstr(header,"RESOLUTION=");
            unsigned width=0,height=0;
            keep=codec&&!strncmp(codec+8,"avc1.",5);
            if(resolution && sscanf(resolution+11,"%ux%u",&width,&height)==2) keep=keep&&width<=1280&&height<=720;
            skip_uri=!keep;free(header);
        } else if(skip_uri && *line!='#' && *line!='\r' && *line!='\n') {keep=0;skip_uri=0;}
        if(keep){memcpy(write,line,len);write+=len;}
        line+=len;
    }
    *write=0;return out;
}
static void webos_hls_loopback(char *playlist) {
    char *p=playlist;
    while((p=strstr(p,"http://localhost:"))){memcpy(p+7,"127.0.0.1",9);p+=17;}
}
#endif
