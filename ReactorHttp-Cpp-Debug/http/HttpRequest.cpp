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
#include <iostream>
using namespace std;

HttpRequest::HttpRequest() {
    reset();
}

HttpRequest::~HttpRequest() {}


void HttpRequest::reset() {
    m_curState = ProcessState::ParseReqLine;
    m_method = m_url = m_version = "";
    m_reqHeaders.clear();
}


void HttpRequest::addHeader(string key, string value) {
    if (key.empty() || value.empty()) return;
    m_reqHeaders.insert(make_pair(key, value));
}


string HttpRequest::getHeader(string key) {
    auto item = m_reqHeaders.find(key);
    if (item == m_reqHeaders.end())
        return string();
    return m_reqHeaders[key];
}

// 辅助解析请求行
char* HttpRequest::splitRequestLine(const char* start, const char* end,
const char* sub, function<void(string)> callback) {
    char* space = const_cast<char*>(end);
    if (sub != NULL) {
        space = (char*)memmem(start, end - start, sub, strlen(sub));
        assert(space != NULL);
    }
    int length = space - start;
    callback(string(start, length));
    return space + 1;
}

// 解析请求行
bool HttpRequest::parseRequestLine(Buffer *readBuf) {
    char* end = readBuf->findCRLF();     // 读出请求行
    char* start = readBuf->data();  // 保存字符串起始地址
    int lineSize = end - start;
    if (lineSize > 0) {   // 解析请求行

        auto methodFunc = bind(&HttpRequest::setMethod, this, placeholders::_1);
        start = splitRequestLine(start, end, " ", methodFunc);

        auto urlFunc = bind(&HttpRequest::seturl, this, placeholders::_1);
        start = splitRequestLine(start, end, " ", urlFunc);

        auto versionFunc = bind(&HttpRequest::setVersion, this, placeholders::_1);
        splitRequestLine(start, end, nullptr, versionFunc);

        readBuf->readPosIncrease(lineSize + 2);   // 为解析请求头做准备
        setState(ProcessState::ParseReqHeaders);  // 修改状态
        
       
        return true;
    }
    return false;
}

// 解析请求头
bool HttpRequest::parseRequestHeader(Buffer *readBuf) {
    char* end = readBuf->findCRLF(); // end指向'\r'
    if (end != nullptr) {
        char* start = readBuf->data();
        int lineSize = end - start;
        char* middle = (char*)memmem(start, lineSize, ": ", 2);  // 基于 ":" 搜索字符串
        if (middle != NULL) {
            int keyLen = middle - start;
            int valueLen = end - middle - 2;
            if (keyLen > 0 && valueLen > 0) {
                string key(start, keyLen);
                string value(middle + 2, valueLen);
                addHeader(key, value);
            }
            readBuf->readPosIncrease(lineSize + 2);
        } else {
            readBuf->readPosIncrease(2);  // 请求头被解析完了, 跳过空行
            setState(ProcessState::ParseReqDone);  // 修改解析状态, 忽略post请求
        }
       
        return true;
    }
    return false;
}

// 解析http请求
bool HttpRequest::parseHttpRequest(Buffer *readBuf, HttpResponse *response, Buffer *sendBuf, int socket) {
    bool flag = true;
    while (m_curState != ProcessState::ParseReqDone) {
        switch (m_curState) {
        case ProcessState::ParseReqLine:
            flag = parseRequestLine(readBuf);
            printf("解析请求行完毕!\n");
            break;
        case ProcessState::ParseReqHeaders:
            flag = parseRequestHeader(readBuf);
            printf("解析请求头完毕!\n");
            break;
        case ProcessState::ParseReqBody:
            break;
        default:
            break;
        }
        if (!flag)
            return flag;
        // 判断是否解析完毕了, 如果完毕了, 需要准备回复的数据
        if (m_curState == ProcessState::ParseReqDone) printf("ParseReqDone\n");
        if (m_curState == ProcessState::ParseReqDone) {
            // 1. 根据解析出的原始数据, 对客户端的请求做出处理
            
            processHttpRequest(response);
            
            // 2. 组织响应数据并发送给客户端
            response->prepareMsg(sendBuf, socket);
        }
    }
    m_curState = ProcessState::ParseReqLine;  // 使状态还原, 服务器能够处理后续的客户端请求
    return flag;
}

// 处理http请求
bool HttpRequest::processHttpRequest(HttpResponse *response) {
    
    cout << m_method << endl;
    if (strcasecmp(m_method.data(), "get") != 0) {
        return -1;
    }
     
    m_url = decodeMsg(m_url);
    // 处理客户端请求的静态资源(目录或文件)
    const char* file = NULL;
    if (strcmp(m_url.data(), "/") == 0) {
        file = "./";
    } else {
        file = m_url.data() + 1;  // 可以移动？
    }
    printf("来到这了!\n");
    // 获得文件的属性
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1) {
        // 文件不存在, 回复404
        response->setFileName("404.html");
        response->setStatusCode(StatusCode::NotFound);
        // 响应头
        response->addHeader("Content-type", getFileType(".html"));
        response->sendDataFunc = sendFile;
        return 0;
    }
    response->setFileName(file);
    response->setStatusCode(StatusCode::OK);

    if (S_ISDIR(st.st_mode)) {
        response->addHeader("Content-type", getFileType(".html"));
        response->sendDataFunc = sendDir;  // 把这个目录中的内容发送给客户端
    } else {
        char tmp[128];
        sprintf(tmp, "%ld", st.st_size);
        response->addHeader("Content-type", getFileType(file));
        response->addHeader("Content-length", to_string(st.st_size));
        response->sendDataFunc = sendFile;  // 把文件的内容发送给客户端
    }   
    return false;
}

// 发送目录
void HttpRequest::sendDir(string dirName, Buffer *sendBuf, int cfd) {
    char buf[4096] = {0};
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName.data());

    struct  dirent** namelist;
    int num = scandir(dirName.data(), &namelist, NULL, alphasort);
    for (int i = 0; i < num; i++) {
        char* name = namelist[i]->d_name;  // 取出文件名
        char subPath[1024] = {0};
        sprintf(subPath, "%s/%s", dirName.data(), name);  // 为什么要拼接？
        
        struct stat st;
        stat(subPath, &st);
        if (S_ISDIR(st.st_mode)) {
            // a标签<a href="">name</a>             
            sprintf(buf + strlen(buf),      
             "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        } else {
            sprintf(buf + strlen(buf),      
             "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);            
        }
        sendBuf->appendString(buf);
#ifndef MSG_SEND_AUTO
        sendBuf->sendData(cfd);
#endif
        memset(buf, 0, sizeof(buf));
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    sendBuf->appendString(buf);
#ifndef MSG_SEND_AUTO
    sendBuf->sendData(cfd);
#endif
    free(namelist);
}

// 发送文件
void HttpRequest::sendFile(string fileName, Buffer *sendBuf, int cfd) {
    
    // 打开文件
    int fd = open(fileName.data(), O_RDONLY);
    assert(fd > 0);
    char buf[1024];
    while (1) {
        int len = read(fd, buf, sizeof buf);
        if (len > 0) {
            // send(cfd, buf, len, 0);
            sendBuf->appendString(buf, len);
        #ifndef MSG_SEND_AUTO
            sendBuf->sendData(cfd);
        #endif
            // usleep(10);  // 这非常重要, 发送端不能发太快, 让接收端喘口气
        } else if (len == 0) {
            close(fd);
            break;
        } else {
            perror("read");
        }
    }
    close(fd);
}

// url解码
string HttpRequest::decodeMsg(string msg) {
    string str = string();
    const char* from = msg.data();
    for (; *from != '\0'; ++from) {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
            str.append(1, hexToDec(from[1]) * 16 + hexToDec(from[2]));
            from += 2;
        } else {
            str.append(1, *from);
        }
    }
    str.append(1, '\0');
    return str;
}

// 得到文件后缀
const string HttpRequest::getFileType(const string name) {
    // 自右向左查找'.'字符, 如果不存在, 返回NULL
    const char* dot = strrchr(name.data(), '.');
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
int HttpRequest::hexToDec(char c) {
    if (c >= '0' && c <= '9') 
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}


