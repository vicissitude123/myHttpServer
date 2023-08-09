#include <stdio.h>
#include <stdlib.h>
#include "TcpConnection.h"

int processRead(void* arg) {
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    int count = bufferSocketRead(conn->readBuf, conn->channel->fd);  // 接收数据
    printf("接收http请求数据: %s\n", conn->readBuf->data + conn->readBuf->readPos);
    if (count > 0) {   // 接收了http请求, 解析http请求
        int socket = conn->channel->fd;
        #ifdef MSG_SEND_AUTO
            writeEventEnable(conn->channel, true);
            eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
        #endif
        bool flag = parseHttpRequest(conn->request, conn->readBuf, conn->response, conn->writeBuf, socket);
        if (!flag) {
            const char* errMsg = "Http/1.1 400 Bad Request\r\n\r\n";   // 解析失败, 回复一个简单的html
            bufferAppendString(conn->writeBuf, errMsg);
        }
    } else {
        #ifndef MSG_SEND_AUTO
            eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
        #endif        
    }
    #ifndef MSG_SEND_AUTO  // 断开连接
        eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
    #endif
    return 0;
}

int processWrite(void* arg) {
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    printf("开始发送数据了...\n");  
    int count = bufferSendData(conn->writeBuf, conn->channel->fd);
    if (count > 0) {
        if (bufferReadableSize(conn->writeBuf) == 0) {              // 判断数据是否被全部发送出去了
            writeEventEnable(conn->channel, false);                 // 1. 不再检测写事件, 修改channel中保存的事件
            eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);  // 2. 修改dispatcher检测的集合--添加任务节点
            eventLoopAddTask(conn->evLoop, conn->channel, DELETE);  // 3. 删除这个节点
        }
    }
    return 0;
}


struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop *evloop) {
    struct TcpConnection* conn = (struct TcpConnection*)malloc(sizeof(struct TcpConnection));
    conn->evLoop = evloop;
    conn->readBuf = bufferInit(10240);
    conn->writeBuf = bufferInit(10240);

    // http
    conn->request = httpRequestInit();
    conn->response = httpResponseInit();
    sprintf(conn->name, "Connection-%d", fd);
    conn->channel = channelInit(fd, ReadEvent, processRead, processWrite, tcpConnectionDestroy, conn);
    eventLoopAddTask(evloop, conn->channel, ADD);

    printf("和客户端建立连接, threadName: %s, threadID: %ld, connName: %s\n", evloop->threadName, evloop->threadID, conn->name);
    return conn;
}

int tcpConnectionDestroy(void* arg) {
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    if (conn != NULL) {
        if (conn->readBuf && bufferReadableSize(conn->readBuf) == 0 &&
            conn->writeBuf && bufferReadableSize(conn->writeBuf) == 0) {
                destroyChannel(conn->evLoop, conn->channel);
                bufferDestroy(conn->readBuf);
                bufferDestroy(conn->writeBuf);
                httpRequestDestroy(conn->request);
                httpResponseDestroy(conn->response);
                free(conn);
            }
    }
    return 0;
}
