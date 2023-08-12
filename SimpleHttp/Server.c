#include "Server.h"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
struct FdInfo {
    int fd;
    int epfd;
    pthread_t tid;
};


int initListenFd(unsigned short port) {
    // 创建监听的fd
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket");
        return -1;
    }

    // 设置端口复用
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (ret == -1) {
        perror("setsockopt");
        return -1;
    }
  
    // 绑定
    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);  // 网络字节序, 大端
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd, (struct sockaddr*)&addr, sizeof addr);
    if (ret == -1) {
        perror("bind");
        return -1;
    }

    // 监听
    ret = listen(lfd, 128);  // 一次最多同时与多少客户端建立连接
    if (ret == -1) {
        perror("listen");
        return -1;
    }
    return lfd;
}

int epollRun(int lfd) {
   
    // 创建epoll实例
    int epfd = epoll_create(1);  // 参数被弃用了
    if (epfd == -1) {
        perror("epoll_create");
        return -1;
    } 
    
    // lfd上树, 即把用于监听的文件描述符添加到epoll树上
    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1) {
        perror("epoll_ctl");
        return -1;
    }
    
    // 检测, 服务器在死循环里检测客户端连接
    struct epoll_event evs[1024];  // 容量不够也不要紧, fd可以分批放入数组, 然后处理
    int size = sizeof(evs) / sizeof(struct epoll_event);
    
    while (1) {
        int num = epoll_wait(epfd, evs, size, -1);
        for (int i = 0; i < num; i++) {
            int fd = evs[i].data.fd;

            struct FdInfo* info = (struct FdInfo*)malloc(sizeof(struct FdInfo));
            info->epfd = epfd;
            info->fd = fd;

            if (fd == lfd) {
                // 建立新连接accpet, 不会阻塞
                // acceptClient(lfd, epfd);
                printf("来了新连接!\n");
                pthread_create(&info->tid, NULL, acceptClient, info);
                printf("新连接创建完成!\n");
            } else {
                // 主要是接收对端的数据, http请求的格式, 请求行, 请求头, 空行, 数据块
                // recvHttpRequest(fd, epfd);
                printf("有链接发送了请求!\n");
                pthread_create(&info->tid, NULL, recvHttpRequest, info);
            }
        }

    }

    return 0;
}

// int acceptClient(int lfd, int epfd)
void* acceptClient(void* arg) {

    struct FdInfo* info = (struct FdInfo*) arg;
    // 建立连接
    int cfd = accept(info->fd, NULL, NULL);
    if (cfd == -1) {
        perror("accept");
        return NULL;
    }

    // 边缘非阻塞效率最高
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    // cfd添加到epoll中, 并设置边缘模式
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;  // 设置边缘模式EPOLLET
    int ret = epoll_ctl(info->epfd, EPOLL_CTL_ADD, cfd, &ev);
    if (ret == -1) {
        perror("epoll_ctl");
        return NULL;
    }
    printf("accpetClient threadId: %ld\n", info->tid);
    free(info);
    return NULL;
}


// int recvHttpRequest(int cfd, int epfd)
void* recvHttpRequest(void *arg) {
    struct FdInfo* info = (struct FdInfo*) arg;

    char buf[4096] = {0};
    char tmp[1024] = {0};
    int len = 0, totle = 0;
    while ((len = recv(info->fd, tmp, sizeof tmp, 0)) > 0) {
        if (totle + len < sizeof buf)
            memcpy(buf + totle, tmp, len);
        totle += len;
    }

    // 判断数据是否被接收完毕
    if (len == -1 && errno == EAGAIN) {
        // 解析请求行
        char* pt = strstr(buf, "\r\n");
        int reqLen = pt - buf;
        buf[reqLen] = '\0';
        parseRequestLine(buf, info->fd);
    } else if (len == 0) {
        // 客户端断开了连接
        epoll_ctl(info->epfd, EPOLL_CTL_DEL, info->fd, NULL);
        close(info->fd);
    } else {
        perror("recv");
    }
    printf("recvMsg threadId: %ld\n", info->tid);
    free(info);
    return NULL;
}


int parseRequestLine(const char *line, int cfd) {
    char method[12];
    char path[1024];
    // 解析请求行 get /xxx/1.jpg http/1.1
    sscanf(line, "%[^ ] %[^ ]", method, path);
    
    if (strcasecmp(method, "get") != 0) {
        return -1;
    }
    decodeMsg(path, path);
    // 处理客户端请求的静态资源(目录或文件)
    const char* file = NULL;
    if (strcmp(path, "/") == 0) {
        file = "./";
    } else {
        file = path + 1;
    }

    // 获得文件的属性
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1) {
        // 文件不存在, 回复404
        sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
        sendFile("404.html", cfd);
        return 0;
    }
    // 判断文件类型
    if (S_ISDIR(st.st_mode)) {
        // 把这个目录中的内容发送给客户端
        sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
        sendDir(file, cfd);
    } else {
        // 把文件的内容发送给客户端
        sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
        sendFile(file, cfd);
    }
    return 0;
}


int sendFile(const char *fileName, int cfd) {
    
    // 打开文件
    int fd = open(fileName, O_RDONLY);
    assert(fd > 0);
    char buf[1024];

#if 0

    while (1) {
        int len = read(fd, buf, sizeof buf);
        if (len > 0) {
            send(cfd, buf, len, 0);
            usleep(10);  // 这非常重要, 发送端不能发太快, 让接收端喘口气
        } else if (len == 0) {
            break;
        } else {
            perror("read");
        }
    }
#else
    
    off_t offset = 0;
    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    while (offset < size) {
        int ret = sendfile(cfd, fd, &offset, size - offset);
        printf("ret value: %d\n", ret);
        if (ret == -1) {
            perror("sendfile");
        }
    }
    sendfile(cfd, fd, NULL, size);
    
#endif
    close(fd);
    return 0;
}


int sendDir(const char *dirName, int cfd) {
    char buf[4096] = {0};
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);

    struct  dirent** namelist;
    int num = scandir(dirName, &namelist, NULL, alphasort);
    for (int i = 0; i < num; i++) {
        char* name = namelist[i]->d_name;  // 取出文件名
        char subPath[1024] = {0};
        sprintf(subPath, "%s/%s", dirName, name);  // 为什么要拼接？
        // sprintf(subPath, "%s", name);

        struct stat st;
        stat(subPath, &st);

        if (S_ISDIR(st.st_mode)) {
            // a标签<a href="">name</a>            要加/         
            sprintf(buf + strlen(buf),      
             "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        } else {
            sprintf(buf + strlen(buf),      
             "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);            
        }
        send(cfd, buf, strlen(buf), 0);  // 发送数据
        memset(buf, 0, sizeof(buf));
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);
    free(namelist);
    return 0;
}


int sendHeadMsg(int cfd, int status, const char *descr, const char *type, int length) {
    // 状态行
    char buf[4096] = {0};
    sprintf(buf, "http/1.1 %d %s\r\n", status, descr);

    // 响应头
    sprintf(buf + strlen(buf), "content-type: %s\r\n", type);
    sprintf(buf + strlen(buf), "content-length: %d\r\n\r\n", length);
    send(cfd, buf, strlen(buf), 0);

    return 0;
}


const char* getFileType(const char *name) {
    // 自右向左查找'.'字符, 如果不存在, 返回NULL
    const char* dot = strrchr(name, '.');
    if (dot == NULL) 
        return "text/plain; charset=utf-8";  // 纯文本;
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "image/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "audio/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";    
    return "text/plain; charset=utf-8";
}

// 将字符转换为整形数
int hexToDec(char c) {
    if (c >= '0' && c <= '9') 
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}


void decodeMsg(char* to, char* from) {
    for (; *from != '\0'; ++to, ++from) {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
            *to = hexToDec(from[1]) * 16 + hexToDec(from[2]);
            from += 2;
        } else {
            *to = *from;
        }
    }
    *to = '\0';
}
