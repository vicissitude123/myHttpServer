#pragma once
#include <string>
#include <map>
#include <functional>
#include "Buffer.h"
#include "HttpResponse.h"
using namespace std;


// 当前的解析状态
enum class ProcessState: char {
    ParseReqLine,
    ParseReqHeaders,
    ParseReqBody,
    ParseReqDone
};

class HttpRequest {
public:

    HttpRequest();
    ~HttpRequest();

    void reset();


    void addHeader(string key, string value);   // 添加请求头
    string getHeader(string key);               // 根据key得到请求头的value
    bool parseRequestLine(Buffer* readBuf);     // 解析请求行
    bool parseRequestHeader(Buffer* readBuf);   // 解析请求头
    bool parseHttpRequest(Buffer* readBuf, HttpResponse* response,
        Buffer* sendBuf, int socket);                  // 解析http请求协议
    bool processHttpRequest(HttpResponse* response);   // 处理http请求协议

    string decodeMsg(string from);                     // 解码字符串
    const string getFileType(const string name);
    
    static void sendDir(string dirName, Buffer* sendBuf, int cfd);
    static void sendFile(string fileNmae, Buffer* sendBuf, int cfd);

    inline void setMethod(string method) {
        m_method = method;
    }
    inline void seturl(string url) {
        m_url = url;
    } 
    inline void setVersion(string version) {
        m_version = version;
    }
    // 获取处理状态
    inline ProcessState getState() {
        return m_curState;
    }         
    inline void setState(ProcessState state) {
        m_curState= state;
    }
private:
    char* splitRequestLine(const char* start, const char* end,
    const char* sub, function<void(string)> callback);
    int hexToDec(char c);
private:
    string m_method;
    string m_url;
    string m_version;
    map<string, string> m_reqHeaders;
    ProcessState m_curState;
};



