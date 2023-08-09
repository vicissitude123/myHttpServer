#pragma once
#include <stdlib.h>
#include "Buffer.h"
#include "Channel.h"
#include "EventLoop.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

// #define MSG_SEND_AUTO
struct TcpConnection {
    struct EventLoop* evLoop;
    struct Channel* channel;
    struct Buffer* readBuf;
    struct Buffer* writeBuf;
    char name[32];
    // http协议
    struct HttpRequest* request;
    struct HttpResponse* response;
};

// 初始化
struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evloop);
int tcpConnectionDestroy(void* arg);