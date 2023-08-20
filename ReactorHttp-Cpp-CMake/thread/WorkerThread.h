#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include "EventLoop.h"
using namespace std;
// 面向对象的优点: 函数名字简化许多, 不用围绕结构体去编写函数, 所有函数都属于类的实例, 起到了封装的效果


// 定义子线程对应的类
class WorkerThread {
public:
    WorkerThread(int index);
    ~WorkerThread();
    // 启动线程
    void run();
    inline EventLoop* getEventLoop() {
        return m_evLoop;
    }
private:
    void running();

private:
    thread* m_thread;  // 保存线程实例
    thread::id m_threadID;
    string m_name;
    mutex m_mutex;  // 互斥锁
    condition_variable m_cond;    // 条件变量
    EventLoop* m_evLoop;
};


