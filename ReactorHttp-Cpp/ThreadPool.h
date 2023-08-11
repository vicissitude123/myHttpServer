#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"
#include <vector>
using namespace std;

// 定义线程池
class ThreadPool {
public:
    ThreadPool(EventLoop* mainLoop, int count);
    ~ThreadPool();
    void run();  // 启动线程池
    EventLoop* takeWorkerEventLoop();  // 取出线程池中的某个子线程的反应堆实例

private:
    EventLoop* m_mainLoop;  // 主线程的反应堆模型
    bool m_isStart;
    int m_threadNum;
    vector<WorkerThread*> m_workerThreads;
    int m_index;
};

