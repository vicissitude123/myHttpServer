#pragma once
#include <string>
#include <sys/epoll.h>
#include "Channel.h"
#include "EventLoop.h"
#include "Dispatcher.h"

using namespace std;

class EpollDispatcher : public Dispatcher {
public:
    // init-- 初始化epoll(epoll_event), poll(pollfd), 或者select(set_fd)需要的数据块
    EpollDispatcher(EventLoop* evloop);              
    
    ~EpollDispatcher();   // 清除数据(关闭fd或释放内存) 
 
    // 添加
    int add() override;
    
    // 删除
    int remove() override;

    // 修改
    int modify() override;
 
    // 事件检测
    int dispatch(int timeout = 2) override;

private:
    int epollCtl(int op);

private:
    int m_epfd;
    struct epoll_event* m_events;
    const int m_maxNode = 520;
};