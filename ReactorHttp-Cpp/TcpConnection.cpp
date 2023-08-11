#include <stdio.h>
#include <stdlib.h>
#include "TcpConnection.h"


int TcpConnection::processRead(void* arg) {
    TcpConnection* conn = (TcpConnection*)arg;
    int socket = conn->m_channel->getSocket();
    int count = conn->m_readBuf->socketRead(socket);  // 接收数据
    printf("接收http请求数据: %s\n", conn->m_readBuf->data());

    if (count > 0) {   // 接收了http请求, 解析http请求
        #ifdef MSG_SEND_AUTO
            conn->m_channel->writeEventEnable(true);
            conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);
        #endif
        bool flag = conn->m_request->parseHttpRequest(conn->m_readBuf, conn->m_response,
        conn->m_writeBuf, socket);
        if (!flag) {
            string errMsg = "Http/1.1 400 Bad Request\r\n\r\n";   // 解析失败, 回复一个简单的html
            conn->m_writeBuf->appendString(errMsg);
        }
    } else {
        #ifdef MSG_SEND_AUTO
            conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
        #endif        
    }
    #ifndef MSG_SEND_AUTO  // 断开连接
        conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
    #endif
    printf("接收完成!\n");
    return 0;
}


int TcpConnection::processWrite(void* arg) {
    printf("开始发送数据了...\n");  
    TcpConnection* conn = (TcpConnection*)arg;
    int count = conn->m_writeBuf->sendData(conn->m_channel->getSocket());
    if (count > 0) {
        if (conn->m_writeBuf->readableSize() == 0) {                  // 判断数据是否被全部发送出去了
            conn->m_channel->writeEventEnable(false);                 // 1. 不再检测写事件, 修改channel中保存的事件
            conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);    // 2. 修改dispatcher检测的集合--添加任务节点
            conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);    // 3. 删除这个节点
        }
    }
    return 0;
}


int TcpConnection::destroy(void* arg) {
    TcpConnection* conn = (TcpConnection*)arg;
    if (conn != nullptr) {
        delete conn;
    }
    return 0;
}


TcpConnection::TcpConnection(int fd, EventLoop* evloop) {
    m_evLoop = evloop;
    m_readBuf = new Buffer(10240);
    m_writeBuf = new Buffer(10240);

    // http
    m_request = new HttpRequest;
    m_response = new HttpResponse;
    m_name = "Connection-" + to_string(fd);

    m_channel = new Channel(fd, FDEvent::ReadEvent, processRead, processWrite, destroy, this);
    evloop->addTask(m_channel, ElemType::ADD);

    printf("和客户端建立连接...\n");
  
}

TcpConnection::~TcpConnection() {
    if (m_readBuf && m_readBuf->readableSize() == 0 &&
        m_writeBuf && m_writeBuf->readableSize() == 0) {
            delete m_readBuf;
            delete m_writeBuf;
            delete m_request;
            delete m_response;
            m_evLoop->freeChannel(m_channel);
        }
}



