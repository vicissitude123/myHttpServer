#pragma once
#include <stdbool.h>
#include "Buffer.h"
#include "HttpResponse.h"
struct RequestHeader {
    char* key;
    char* value;
};

// 当前的解析状态
enum HttpRequestState {
    ParseReqLine,
    ParseReqHeaders,
    ParseReqBody,
    ParseReqDone
};

struct HttpRequest {
    char* method;
    char* url;
    char* version;
    struct RequestHeader* reqHeaders;
    int reqHeadersNum;
    enum HttpRequestState curState;
};


struct HttpRequest* httpRequestInit();
void httpRequestReset(struct HttpRequest* req);
void httpRequestResetEx(struct HttpRequest* req);
void httpRequestDestroy(struct HttpRequest* req);

// 获取处理状态
enum HttpRequestState HttpRequestState(struct HttpRequest* request);

// 添加请求头
void httpRequestAddHeader(struct HttpRequest* request, const char* key, const char* value);

// 根据key得到请求头的value
char* httpRequestGetHeader(struct HttpRequest* request, const char* key);

// 解析请求行
bool parseHttpRequestLine(struct HttpRequest* request, struct Buffer* readBuf);

// 解析请求头
bool parseHttpRequestHeader(struct HttpRequest* request, struct Buffer* readBuf);

// 解析http请求协议
bool parseHttpRequest(struct HttpRequest* request, struct Buffer* readBuf, struct HttpResponse* response,
struct Buffer* sendBuf, int socket);

// 处理http请求协议
bool processHttpRequest(struct HttpRequest* request,  struct HttpResponse* response);

void sendDir(const char* dirName, struct Buffer* sendBuf, int cfd);

void sendFile(const char* fileNmae, struct Buffer* sendBuf, int cfd);
// 解码字符串
void decodeMsg(char* to, char* from);

int hexToDec(char c);

const char* getFileType(const char *name);