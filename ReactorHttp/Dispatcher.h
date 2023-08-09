#pragma once
#include <stdio.h>
#include "Channel.h"
#include "EventLoop.h"


// struct EventLoop;
struct Dispatcher {
     
    // init-- 初始化epoll(epoll_event), poll(pollfd), 或者select(set_fd)需要的数据块
    void* (*init)();
    
    // 添加
    int (*add)(struct Channel* channel, struct EventLoop* evLoop);
    
    // 删除
    int (*remove)(struct Channel* channel, struct EventLoop* evLoop);

    // 修改
    int (*modify)(struct Channel* channel, struct EventLoop* evLoop);
 
    // 事件检测
    int (*dispatch)(struct EventLoop* evLoop, int timeout);

    // 清除数据(关闭fd或释放内存) 
    int (*clear)(struct EventLoop* evLoop);
};