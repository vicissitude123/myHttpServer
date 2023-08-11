#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "Server.h"

int main(int argc, char* argv[]) {
    
    if (argc < 3) {
        printf("./a.out port path\n");
        return -1;
    }
    
    unsigned short port = atoi(argv[1]);

    // 初始化用于监听的套接字
    int lfd = initListenFd(port);

    // 切换服务器的工作路径, 当前服务器的进程切换到用户指定的目录下面
    chdir(argv[2]);

    // 启动服务器程序
    epollRun(lfd);

    return 0;
}