#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include <string>

using namespace std;

class EventLoop; 

class Dispatcher {
public:
    
    // init-- 初始化epoll(epoll_event), poll(pollfd), 或者select(set_fd)需要的数据块
    Dispatcher(EventLoop* evloop);              
    
    virtual ~Dispatcher();   // 清除数据(关闭fd或释放内存) 
 
    // 添加
    virtual int add();
    
    // 删除
    virtual int remove();

    // 修改
    virtual int modify();
 
    // 事件检测
    virtual int dispatch(int timeout = 2);


    inline void setChannel(Channel* channel) {
        m_channel = channel;
    }

protected:
    string m_name = string();
    Channel* m_channel;
    EventLoop* m_evLoop;
};