#pragma once
#include <string>
using namespace std;
class Buffer {                               
public:                         
    Buffer(int size);            
    ~Buffer();                   
                                 
    void extendRoom(int size);   
    inline int writeableSize() { 
        return m_capacity - m_writePos;
    }        
    inline int readableSize() {  // 获得可读
        return m_writePos - m_readPos;
    }       

    // 写内存 1.直接写 2.接收套接字数据
    int appendString(const char* data, int size);
    int appendString(const char* data);
    int appendString(const string data);
    int socketRead(int fd);
    int sendData(int socket);      // 发送数据

    char* findCRLF();  // 根据\r\n取出一行, 找到其在数据块中的位置, 返回该位置

    inline char* data() {
        return m_data + m_readPos;
    }
    inline int readPosIncrease(int count) {
        m_readPos += count;
        return m_readPos;
    }
private:
    char* m_data;  // 指向内存的指针
    int m_capacity;
    int m_readPos = 0;
    int m_writePos = 0;
};



