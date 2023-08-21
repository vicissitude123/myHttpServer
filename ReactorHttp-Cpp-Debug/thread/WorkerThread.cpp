#include "WorkerThread.h"
#include <stdio.h>


// 子线程的回调函数
void WorkerThread::running() {
    m_mutex.lock();
    m_evLoop = new EventLoop(m_name);
    m_mutex.unlock();
    m_cond.notify_one();
    m_evLoop->run();
}


WorkerThread::WorkerThread(int index) {
    m_evLoop = nullptr;
    m_thread = nullptr;
    m_threadID = thread::id();
    m_name = "SubThread-" + to_string(index);
}

WorkerThread::~WorkerThread() {
    if (m_thread != nullptr) {
        delete m_thread;
    }
}

void WorkerThread::run() {
    m_thread = new thread(&WorkerThread::running, this);   // 创建子线程
    unique_lock<mutex> locker(m_mutex);
    while (m_evLoop == nullptr) {
        m_cond.wait(locker);
    }
}

