#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "TcpServer.h"

int main(int argc, char* argv[]) {  

    if (argc < 3) {
        printf("./a.out port path\n");
        return -1;
    }
    unsigned short port = atoi(argv[1]);

    // 切换服务器的工作路径, 当前服务器的进程切换到用户指定的目录下面
    chdir(argv[2]);

    // 启动服务器
    struct TcpServer* server = tcpServerInit(port, 4);
    tcpServerRun(server);
    return 0;
}