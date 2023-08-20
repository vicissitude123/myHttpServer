#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include "Buffer.h"
using namespace std;


Buffer::Buffer(int size) : m_capacity(size) {
    m_data =  (char*)malloc(size);
    bzero(m_data, size);
}

Buffer::~Buffer() {
    if (m_data != nullptr) {
        free(m_data);
    }
}

void Buffer::extendRoom(int size) {
    // 1. 内存够用, 不需要扩容
    if (writeableSize() >= size) {
        return;
    }
    // 2. 内存需要合并才够用, 不需要扩容
    else if (m_readPos + writeableSize() >= size) {
        int readable = readableSize();  
        memcpy(m_data, m_data + m_readPos, readable);   // 移动内存
        m_readPos = 0;
        m_writePos = readable;
    // 3. 内存不够用, 扩容
    } else {  
        void* temp = realloc(m_data, m_capacity + size);
        if (temp == NULL) {
            return;
        }
        memset((char*)temp + m_capacity, 0, size);
        m_data = static_cast<char*>(temp);
        m_capacity += size;
    }
}


int Buffer::appendString(const char *data, int size) {
    if (data == nullptr || size <= 0) return -1;
    extendRoom(size);
    memcpy(m_data + m_writePos, data, size);
    m_writePos += size;
    return 0;
}

int Buffer::appendString(const char *data) {
    int size = strlen(data);
    int ret = appendString(data, size);
    return ret;    
}

int Buffer::appendString(const string data) {
    int ret = appendString(data.data());
    return ret;
}

int Buffer::socketRead(int fd) {

    struct iovec vec[2];   // read/recv/readv
    int writeable = writeableSize();
    vec[0].iov_base = m_data + m_writePos;
    vec[0].iov_len = writeable;
    char* tmpbuf = (char*)malloc(40960);
    vec[1].iov_base = m_data + m_writePos;
    vec[1].iov_len = 40960;
    int result = readv(fd, vec, 2);


    if (result == -1) {
        return -1;
    } else if (result <= writeable) {
        m_writePos += result;
    } else {
        m_writePos = m_capacity;      
        appendString(tmpbuf, result - writeable);
    }
    free(tmpbuf);
    return result;
}


int Buffer::sendData(int socket) {
    // 判断有无数据
    int readable = readableSize();
    if (readable > 0) {
        int count = send(socket, m_data + m_readPos, readable, MSG_NOSIGNAL);
        if(count > 0) {
            m_readPos += count;
            usleep(1);
        }
        return count;
    }
    return 0;    
}


char *Buffer::findCRLF() {
    // strstr --> 大字符串中匹配子字符串(遇到\0结束)
    // memmem --> 大数据块中匹配子数据块(需要指定数据块的大小)
    char* ptr = (char*)memmem(m_data + m_readPos, readableSize(), "\r\n", 2);
    return ptr;
}

