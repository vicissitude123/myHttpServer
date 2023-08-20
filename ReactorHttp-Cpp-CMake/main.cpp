#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "TcpServer.h"


// 1. 复习一些函数
// 关于fopen, fwrite, fprintf, fscanf
// 关于sprintf和sscanf
// 关于字符串匹配有一套函数, strstr, memmem
// 关于字符串复制有一套函数, memcpy, memmove, strcpy, strncpy
// 关于扩容有一套函数, malloc, calloc, realloc, free
// 关于信息的发送与接收有一套函数, read, write, send recv, readv

// 2. 学习C++新特性

// 3. 深刻理解多态

// 4. 深刻理解多线程, 以及IO多路复用模型

// 5. 了解HTTP通信, 并能够进行封装



// 功能, 数据, 结构体, 处理函数
int main(int argc, char* argv[]) {  

    if (argc < 3) {
        printf("./a.out port path\n");
        return -1;
    }
    unsigned short port = atoi(argv[1]);

    // 切换服务器的工作路径, 当前服务器的进程切换到用户指定的目录下面
    chdir(argv[2]);

    // 启动服务器
    TcpServer* server = new TcpServer(port, 4);
    server->run();
    return 0;
}