#ifndef ROCKET_TCP_BUFFER_H
#define ROCKET_TCP_BUFFER_H

#include <vector>
#include <memory>

namespace rocket {

    class TcpBuffer {
    public:
        typedef std::shared_ptr<TcpBuffer> s_ptr;

        TcpBuffer(int size);

        ~TcpBuffer();

        int readAble(); // 返回可读字节数

        int writeAble(); // 返回可写字节数

        int readIndex();

        int writeIndex();

        void writeToBuffer(const char* buf, int size);

        void readFromBuffer(std::vector<char>& re, int size);

        void resizeBuffer(int new_size);

        void adjustBuffer(); // 调整m_read_index回m_buffer的起点

        void moveReadIndex(int size);

        void moveWriteIndex(int size);

    private:
        int m_read_index {0}; // 当前可读位置起点
        int m_write_index {0}; // 当前可读位置终点
        int m_size {0};

    public:
        std::vector<char> m_buffer;
    };

}


#endif //ROCKET_TCP_BUFFER_H
