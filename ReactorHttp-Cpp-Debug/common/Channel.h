#pragma once
#include <functional>
using namespace std;
// typedef int(*handleFunc)(void* arg);
// using handleFunc = int(*)(void*);  // C++风格函数指针

enum class FDEvent {
    TimeOut    = 0x01,
    ReadEvent  = 0x02,
    WriteEvent = 0x04,
};

class Channel {
public:
    // 可调用对象包装器打包的是什么? 1.函数指针  2.可调用对象(可以像函数一样使用)
    // 最终得到了地址, 但是没有调用
    using handleFunc = function<int(void*)>;

    Channel(int fd, FDEvent events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg);

    // 修改fd的写事件
    void writeEventEnable(bool flag);

    // 判断是否需要检测文件描述符的写事件
    bool isWriteEventEnable();

    // 回调函数
    handleFunc readCallback; 
    handleFunc writeCallback;
    handleFunc destroyCallback;
    // 取出私有成员的值

    inline int getEvent() {
        return m_events;
    }
    inline int getSocket() {
        return m_fd;
    }
    inline const void* getArg() {  // 不准修改指针对应的值
        return m_arg;
    }

private:
    int m_fd;      // 文件描述符
    int m_events;  // 事件
    void* m_arg;   // 回调函数参数
};


