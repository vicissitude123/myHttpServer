#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include "HttpRequest.h"
#define HeaderSize 12

struct HttpRequest* httpRequestInit() {
    struct HttpRequest* request = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
    httpRequestReset(request);
    request->reqHeaders = (struct RequestHeader*)malloc(sizeof(struct RequestHeader) * HeaderSize);
    return request;
}

void httpRequestReset(struct HttpRequest *req) {
    req->curState = ParseReqLine;
    req->method = NULL;
    req->url = NULL;
    req->version = NULL;
    req->reqHeadersNum = 0;
}

void httpRequestResetEx(struct HttpRequest *req) {
    free(req->url);
    free(req->method);
    free(req->version);
    if (req->reqHeaders != NULL) {
        for (int i = 0; i < req->reqHeadersNum; i++) {
            free(req->reqHeaders[i].key);
            free(req->reqHeaders[i].value);
        }
        free(req->reqHeaders);
    }
    httpRequestReset(req);
}

void httpRequestDestroy(struct HttpRequest *req) {
    if (req != NULL) {
        httpRequestResetEx(req);
        free(req);
    }

}

enum HttpRequestState HttpRequestState(struct HttpRequest* request) {
    return request->curState;
}

void httpRequestAddHeader(struct HttpRequest* request, const char* key, const char* value) {
    request->reqHeaders[request->reqHeadersNum].key = (char*)key;
    request->reqHeaders[request->reqHeadersNum].value = (char*)value;
    request->reqHeadersNum++;
}

char* httpRequestGetHeader(struct HttpRequest* request, const char* key) {
    if (request != NULL) {
        for (int i = 0; i < request->reqHeadersNum; i++) {
            if (strncasecmp(request->reqHeaders[i].key, key, strlen(key)) == 0) {
                request->reqHeaders[i].value;
            }
        }
    }
    return NULL;
}


char* splitRequestLine(const char* start, const char* end, const char* sub, char** ptr) {
    char* space = (char*)end;
    if (sub != NULL) {
        space = (char*)memmem(start, end - start, sub, strlen(sub));
        assert(space != NULL);
    }
    int length = space - start;
    char* tmp = (char*)malloc(length + 1);
    strncpy(tmp, start, length);
    tmp[length] = '\0';
    *ptr = tmp;
    return space + 1;
}



bool parseHttpRequestLine(struct HttpRequest *request, struct Buffer *readBuf) {
    // 读出请求行
    // 保存字符串起始地址
    char* end = bufferFindCRLF(readBuf);
    char* start = readBuf->data + readBuf->readPos;
    int lineSize = end - start;
    if (lineSize) {

        // 解析请求行
        start = splitRequestLine(start, end, " ", &request->method);
        start = splitRequestLine(start, end, " ", &request->url);
        splitRequestLine(start, end, NULL, &request->version);
        
        // 为解析请求头做准备
        readBuf->readPos += lineSize;
        readBuf->readPos += 2;

        // 修改状态
        request->curState = ParseReqHeaders;
        return true;
    }
    return false;
}

bool parseHttpRequestHeader(struct HttpRequest *request, struct Buffer *readBuf) {
    char* end = bufferFindCRLF(readBuf); // end指向'\r'
    if (end != NULL ) {
        char* start = readBuf->data + readBuf->readPos;
        int lineSize = end - start;
        // 基于 ":" 搜索字符串
        char* middle = (char*)memmem(start, lineSize, ": ", 2);
        if (middle != NULL) {
            char* key = (char*)malloc(middle - start + 1);
            strncpy(key, start, middle - start);
            key[middle - start] = '\0';

            char* value = (char*)malloc(end - middle - 2 + 1);
            strncpy(value, middle + 2, end - middle - 2);
            value[end - middle - 2] = '\0';

            httpRequestAddHeader(request, key, value);
            readBuf->readPos += lineSize;
            readBuf->readPos += 2;
        } else {
            // 请求头被解析完了, 跳过空行
            readBuf->readPos += 2;
            
            // 修改解析状态, 忽略post请求
            request->curState = ParseReqDone;
        }

        return true;
    }
    return false;
}

bool parseHttpRequest(struct HttpRequest *request, struct Buffer *readBuf, struct HttpResponse* response,
struct Buffer* sendBuf, int socket) {
    bool flag = true;

    while (request->curState != ParseReqDone) {
        switch (request->curState) {
        case ParseReqLine:
            flag = parseHttpRequestLine(request, readBuf);
            break;
        case ParseReqHeaders:
            flag = parseHttpRequestHeader(request, readBuf);
        case ParseReqBody:
            break;
        default:
            break;
        }
        if (!flag)
            return flag;
        // 判断是否解析完毕了, 如果完毕了, 需要准备回复的数据
        if (request->curState == ParseReqDone) {
            // 1. 根据解析出的原始数据, 对客户端的请求做出处理
            processHttpRequest(request, response);
            // 2. 组织响应数据并发送给客户端
            httpResponsePrepareMsg(response, sendBuf, socket);
        }
    }
    request->curState = ParseReqLine;  // 使状态还原, 服务器能够处理后续的客户端请求
    return flag;
}

bool processHttpRequest(struct HttpRequest *request, struct HttpResponse* response) {    
    if (strcasecmp(request->method, "get") != 0) {
        return -1;
    }
    decodeMsg(request->url, request->url);
    // 处理客户端请求的静态资源(目录或文件)
    const char* file = NULL;
    if (strcmp(request->url, "/") == 0) {
        file = "./";
    } else {
        file = request->url + 1;
    }

    // 获得文件的属性
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1) {
        // 文件不存在, 回复404
        strcpy(response->fileName, "404.html");
        response->statusCode = NotFount;
        strcpy(response->statusMsg, "Not Found");
        
        // 响应头
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));
        response->sendDataFunc = sendFile;
        return 0;
    }
    strcpy(response->fileName, file);
    response->statusCode = OK;
    strcpy(response->statusMsg, "OK");
    if (S_ISDIR(st.st_mode)) {
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));
        response->sendDataFunc = sendDir;  // 把这个目录中的内容发送给客户端
    } else {
        char tmp[128];
        sprintf(tmp, "%ld", st.st_size);
        httpResponseAddHeader(response, "Content-type", getFileType(file));
        httpResponseAddHeader(response, "Content-length", tmp);
        response->sendDataFunc = sendFile;  // 把文件的内容发送给客户端
    }   
    return false;
}


int sendFile(const char *fileName, struct Buffer* sendBuf, int cfd) {
    
    // 打开文件
    int fd = open(fileName, O_RDONLY);
    assert(fd > 0);
    char buf[1024];

#if 1

    while (1) {
        int len = read(fd, buf, sizeof buf);
        if (len > 0) {
            // send(cfd, buf, len, 0);
            bufferAppendData(sendBuf, buf, len);
        #ifndef MSG_SEND_AUTO
            bufferSendData(sendBuf, cfd);
        #endif
            // usleep(10);  // 这非常重要, 发送端不能发太快, 让接收端喘口气
        } else if (len == 0) {
            close(fd);
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


int sendDir(const char *dirName, struct Buffer* sendBuf, int cfd) {
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
        bufferAppendString(sendBuf, buf);
#ifndef MSG_SEND_AUTO
        bufferSendData(sendBuf, cfd);
#endif
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    bufferAppendString(sendBuf, buf);
#ifndef MSG_SEND_AUTO
    bufferSendData(sendBuf, cfd);
#endif
    free(namelist);
    return 0;
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
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt"))
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