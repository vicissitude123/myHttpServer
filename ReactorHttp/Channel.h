#pragma once

#include <stdbool.h>

enum FDEvent {
    TimeOut    = 0x01,
    ReadEvent  = 0x02,
    WriteEvent = 0x04,
};

typedef int(*handleFunc)(void* arg);

struct Channel {
    int fd;      // 文件描述符
    int events;  // 事件
    handleFunc readCallback; 
    handleFunc writeCallback;
    handleFunc destroyCallback;
    void* arg;   // 回调函数参数
};

// 初始化一个Channel
struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg);

// 修改fd的写事件
void writeEventEnable(struct Channel* channel, bool flag);

// 判断是否需要检测文件描述符的写事件
bool isWriteEventEnable(struct Channel* channel);

