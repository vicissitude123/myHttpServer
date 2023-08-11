#pragma once
#include <string>
#include <sys/select.h>
#include "Channel.h"
#include "EventLoop.h"
#include "Dispatcher.h"

using namespace std;

class SelectDispatcher : public Dispatcher {
public:
    // init-- 初始化epoll(epoll_event), poll(pollfd), 或者select(set_fd)需要的数据块
    SelectDispatcher(EventLoop* evloop);              
    
    ~SelectDispatcher();   // 清除数据(关闭fd或释放内存) 
 
    // 添加
    int add() override;
    
    // 删除
    int remove() override;

    // 修改
    int modify() override;
 
    // 事件检测
    int dispatch(int timeout = 2) override;

private:
    void setFdSet();
    void clearFdSet();

private:
    fd_set m_readSet;
    fd_set m_writeSet;
    const int m_maxSize = 1024;
};