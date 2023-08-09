#include <assert.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "EventLoop.h"

// dispatch用到了回调
// channel用到了回调
// 与客户端建立连接也用到了回调

// 写数据
void taskWakeup(struct EventLoop* evLoop) {
    const char* msg = "唤醒";
    write(evLoop->socketPair[0], msg, strlen(msg));
}

// 读数据
int readLocalMessage(void* arg) {
    struct EventLoop* evLoop = (struct EventLoop*)arg;
    char buf[256];
    read(evLoop->socketPair[1], buf, sizeof(buf));
    return 0;
}

struct EventLoop *eventLoopInit() {
    return eventLoopInitEx(NULL);
}

// 反应堆初始化
struct EventLoop *eventLoopInitEx(const char *threadName) {
    struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
    evLoop->isQuit           = false;
    evLoop->threadID         = pthread_self();
    evLoop->dispatcher       = &EpollDispatcher;            // 最关键的
    evLoop->dispatcherData   = evLoop->dispatcher->init();
    evLoop->head             = evLoop->tail = NULL;         // 链表
    evLoop->channelMap       = channelMapInit(128);         // map
    pthread_mutex_init(&evLoop->mutex, NULL);
    strcpy(evLoop->threadName, threadName == NULL ? "MainThread" : threadName);
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, evLoop->socketPair);
    if (ret == -1) {
        perror("socketpair");
        exit(0);
    } 
    // 指定规则: [0]发送数据, [1]接收数据
    struct Channel* channel = channelInit(evLoop->socketPair[1], ReadEvent, readLocalMessage, NULL, NULL, evLoop);
    eventLoopAddTask(evLoop, channel, ADD);                  // channel 添加到任务队列
    return evLoop;
}

// 启动反应堆
int eventLoopRun(struct EventLoop *evLoop) {
    assert(evLoop != NULL);
    struct Dispatcher* dispatcher = evLoop->dispatcher;  // 取出事件分发和模型检测
    if (evLoop->threadID != pthread_self()) return -1;   // 比较线程ID是否正常
    
    // 循环进行事件处理
    while (!evLoop->isQuit) {
        dispatcher->dispatch(evLoop, 2);  // 超时时长 2s
        eventLoopProcessTask(evLoop);
    }
    return 0;
}


int eventActivate(struct EventLoop *evLoop, int fd, int event) {
    if (fd < 0 || evLoop == NULL) 
        return -1;
    struct Channel* channel = evLoop->channelMap->list[fd];
    assert(channel->fd == fd);
    if ((event & ReadEvent) && channel->readCallback) 
        channel->readCallback(channel->arg);
    if ((event & WriteEvent) && channel->writeCallback) 
        channel->writeCallback(channel->arg);
    return 0;
}


int eventLoopAddTask(struct EventLoop *evLoop, struct Channel *channel, int type) {
    pthread_mutex_lock(&evLoop->mutex);  // 加锁, 保护共享资源
    struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));   // 创建新节点
    node->channel = channel;
    node->type = type;
    node->next = NULL;
    if (evLoop->head == NULL) {
        evLoop->head = evLoop->tail = node;
    } else {
        evLoop->tail->next = node;
        evLoop->tail = node;
    }
    pthread_mutex_unlock(&evLoop->mutex);
    // 处理节点
    /**
     * 细节:
     *  1. 对于链表节点的添加：可能是当前线程也可能是其它线程(主线程)
     *     (1) 修改fd的事件, 当前子线程发起, 当前子线程处理
     *     (2) 添加新的fd, 添加任务节点的操作是由主线程发起的 
     * 2. 不能让主线程处理任务队列, 需要由当前的子线程处理
    */
    if (evLoop->threadID == pthread_self()) {
        eventLoopProcessTask(evLoop);  // 当前子线程
    } else {
        taskWakeup(evLoop);            // 主线程 -- 去告诉子线程处理任务队列中的任务 // 1. 子线程在工作 2.子线程被阻塞了, select, poll, epoll
    }
    return 0;
}




int eventLoopProcessTask(struct EventLoop* evLoop) {
    pthread_mutex_lock(&evLoop->mutex);
    struct ChannelElement* head = evLoop->head;   // 取出头节点
    while (head != NULL) {
        struct Channel* channel = head->channel;
        if (head->type == ADD) {     // 添加
            eventLoopAdd(evLoop, channel);
        }
        if (head->type == DELETE) {  // 删除
            eventLoopRemove(evLoop, channel);
        }
        if (head->type == MODIFY) {  // 修改
            eventLoopModify(evLoop, channel);
        }
        struct ChannelElement* tmp = head;
        head = head->next;
        free(tmp);
    }
    evLoop->head = evLoop->tail = NULL;
    pthread_mutex_unlock(&evLoop->mutex);
    return 0;
}


int eventLoopAdd(struct EventLoop *evLoop, struct Channel *channel) {
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size) {
        if (!makeMapRoom(channelMap, fd, sizeof(struct Channel*))) {
            return -1;
        }
    }
    // 核心代码
    if (channelMap->list[fd] == NULL) {
        channelMap->list[fd] = channel;
        evLoop->dispatcher->add(channel, evLoop);
    }
    return 0;
}


int eventLoopRemove(struct EventLoop *evLoop, struct Channel *channel) {
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size) {
        return -1;
    }    
    int ret = evLoop->dispatcher->remove(channel, evLoop);
    return ret;
}


int eventLoopModify(struct EventLoop *evLoop, struct Channel *channel) {
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (channelMap->list[fd] == NULL) {
        return -1;
    }
    int ret = evLoop->dispatcher->modify(channel, evLoop);
    return ret;
}


int destroyChannel(struct EventLoop *evLoop, struct Channel *channel) {
    evLoop->channelMap->list[channel->fd] = NULL;  // 删除channel和fd的对应关系
    close(channel->fd);
    free(channel); 
    return 0;
}
