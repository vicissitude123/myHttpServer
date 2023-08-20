#pragma once
#include <stdlib.h>
#include <string>
#include <map>
#include <functional>
#include "Buffer.h"
using namespace std;

// 定义状态码枚举
enum class StatusCode {
    Unknown,
    OK = 200,
    MovedPermanently = 301,
    MovedTemporarily = 302,
    BadRequest = 400,
    NotFound = 404,
};


// 定义结构体
class HttpResponse {

public:

    HttpResponse();
    ~HttpResponse();

    function<void(const string, struct Buffer*, int)> sendDataFunc;
    void addHeader(const string key, const string value);   // 添加响应头
    void prepareMsg(Buffer* sendBuf, int socket);  // 组织http响应数据
 
    inline void setFileName(string name) {
        m_fileName = name;
    }
    inline void setStatusCode(StatusCode code) {
        m_statusCode = code;
    }

private:
    StatusCode m_statusCode;  // 状态码
    string m_fileName;
    map<string, string> m_headers;
    const map<int, string> m_info = {
        {200, "OK"},
        {301, "MovedPermanently"},
        {302, "MovedTemporarily"},
        {400, "BadRequest"},
        {404, "NotFound"},
    };
};
