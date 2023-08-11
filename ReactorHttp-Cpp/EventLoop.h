#pragma once
#include <queue>
#include <map>
#include <thread>
#include <mutex>
#include "Channel.h"
#include "Dispatcher.h" 
using namespace std;

// 处理该节点中的channel的方式
enum class ElemType:char{ADD, DELETE, MODIFY};

// 定义任务队列的节点
struct ChannelElement {
    ElemType type;  // 如何处理该节点中的channel
    Channel* channel;
};

class Dispatcher;

class EventLoop {
public:
    EventLoop();
    EventLoop(const string threadName);
    ~EventLoop();

    // 启动反应堆模型
    int run();

    // 处理被激活的文件fd
    int eventActivate(int fd, int event);

    // 添加任务到任务队列
    int addTask(Channel* channel, ElemType type);

    // 处理任务队列中的任务
    int processTaskQ();

    // 处理dispatcher中的节点
    int add(Channel* channel);
    int remove(Channel* channel);
    int modify(Channel* channel);

    // 释放channel
    int freeChannel(Channel* channel);

    int readMessage();
    inline thread::id getThreadID() {
        return m_threadID;
    }

    static int readLocalMessage(void* arg);

private:
    void taskWakeup();

private:
    bool m_isQuit;
    Dispatcher* m_dispatcher;  // 该指针指向子类的实例
    queue<ChannelElement*> m_taskQ;
    map<int, Channel*> m_channelMap;

    thread::id m_threadID;     // 线程id 
    string m_threadName;
    
    mutex m_mutex;
    int m_socketPair[2];
};
