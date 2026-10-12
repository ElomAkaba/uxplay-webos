#include "hls_bridge.h"
#include <iostream>
int main(int argc,char**argv){if(argc!=3)return 2;std::cout<<merged_hls_url(argv[1],argv[2]);}
