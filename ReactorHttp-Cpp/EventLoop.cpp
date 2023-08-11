#include <assert.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "EventLoop.h"
#include "Channel.h"
#include "SelectDispatcher.h"
#include "PollDispatcher.h"
#include "EpollDispatcher.h"
// dispatch用到了回调
// channel用到了回调
// 与客户端建立连接也用到了回调

// 写数据
void EventLoop::taskWakeup() {
    const char* msg = "唤醒";
    write(m_socketPair[0], msg, strlen(msg));
}


int EventLoop::readLocalMessage(void* arg) {
    EventLoop* evLoop = static_cast<EventLoop*>(arg);
    char buf[256];
    read(evLoop->m_socketPair[1], buf, sizeof(buf));
    return 0;
}


int EventLoop::freeChannel(Channel *channel) {
    auto it = m_channelMap.find(channel->getSocket()); // 删除channel和fd的对应关系
    if (it != m_channelMap.end()) {
        m_channelMap.erase(it);
        close(channel->getSocket());
        delete channel;
    }
    return 0;
}


int EventLoop::readMessage()
{
    char buf[256];
    read(m_socketPair[1], buf, sizeof(buf));    
    return 0;
}


// 委托构造函数
EventLoop::EventLoop():EventLoop(string()) {}


EventLoop::EventLoop(const string threadName) {
    m_threadName = threadName == string() ? "MainThread" : threadName;
    m_isQuit     = false;
    m_threadID   = this_thread::get_id();
    m_dispatcher = new EpollDispatcher(this);         // 最关键的
    m_channelMap.clear();
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_socketPair);
    if (ret == -1) {
        perror("socketpair");
        exit(0);
    } 
#if 0
    // 指定规则: [0]发送数据, [1]接收数据
    Channel* channel = new Channel(m_socketPair[1], FDEvent::ReadEvent, readLocalMessage, nullptr, nullptr, this);
#else
    auto obj = bind(&EventLoop::readMessage, this);
    Channel* channel = new Channel(m_socketPair[1], FDEvent::ReadEvent, obj, nullptr, nullptr, this);
#endif
    addTask(channel, ElemType::ADD);                  // channel 添加到任务队列
}


EventLoop::~EventLoop() {}


int EventLoop::run() {
    m_isQuit = false;
    if (m_threadID != this_thread::get_id()) return -1;   // 比较线程ID是否正常
    while (!m_isQuit) {  // 循环进行事件处理
        m_dispatcher->dispatch(2);  // 超时时长 2s
        processTaskQ();
    }
    return 0;
}


int EventLoop::eventActivate(int fd, int event) {
    if (fd < 0) return -1;
    Channel* channel = m_channelMap[fd];
    assert(channel->getSocket() == fd);
    if (event & (int)FDEvent::ReadEvent && channel->readCallback) 
        channel->readCallback(const_cast<void*>(channel->getArg()));
    if (event & (int)FDEvent::WriteEvent && channel->writeCallback) 
        channel->writeCallback(const_cast<void*>(channel->getArg()));
    return 0;
}


int EventLoop::addTask(Channel *channel, ElemType type) {
    m_mutex.lock();  // 加锁, 保护共享资源
    ChannelElement* node = new ChannelElement;   // 创建新节点
    node->channel = channel;
    node->type = type;
    m_taskQ.push(node);
    m_mutex.unlock();
    // 处理节点
    /**
     * 细节:
     *  1. 对于链表节点的添加：可能是当前线程也可能是其它线程(主线程)
     *     (1) 修改fd的事件, 当前子线程发起, 当前子线程处理
     *     (2) 添加新的fd, 添加任务节点的操作是由主线程发起的 
     * 2. 不能让主线程处理任务队列, 需要由当前的子线程处理
    */
    if (m_threadID == this_thread::get_id()) {
        processTaskQ();  // 当前子线程
    } else {
        taskWakeup(); // 主线程 -- 去告诉子线程处理任务队列中的任务 // 1. 子线程在工作 2.子线程被阻塞了, select, poll, epoll
    }
    return 0;    
}


int EventLoop::processTaskQ() {
    while (!m_taskQ.empty()) {
        m_mutex.lock();
        ChannelElement* node = m_taskQ.front(); // 取出头节点
        m_taskQ.pop();
        m_mutex.unlock();
        Channel* channel = node->channel;
        if (node->type == ElemType::ADD)      // 添加
            add(channel);
        if (node->type == ElemType::DELETE)   // 删除
            remove(channel);
        if (node->type == ElemType::MODIFY)   // 修改
            modify(channel);
        delete node;
    }
    return 0;
}


int EventLoop::add(Channel *channel) {
    int fd = channel->getSocket();
    if (m_channelMap.find(fd) == m_channelMap.end()) {   // 核心代码
        m_channelMap.insert(make_pair(fd, channel));
        m_dispatcher->setChannel(channel);
        int ret = m_dispatcher->add();
        return ret;
    }
    return -1;
}


int EventLoop::remove(Channel *channel) {
    int fd = channel->getSocket();
    if (m_channelMap.find(fd) == m_channelMap.end()) {
        return -1;
    }    
    m_dispatcher->setChannel(channel);
    int ret = m_dispatcher->remove();
    return ret;
}


int EventLoop::modify(Channel *channel) {
    int fd = channel->getSocket();
    if (m_channelMap.find(fd) == m_channelMap.end()) {
        return -1;
    }
    m_dispatcher->setChannel(channel);
    int ret = m_dispatcher->modify();
    return ret;

}

