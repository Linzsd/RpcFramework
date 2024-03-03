#ifndef ROCKET_ABSTRACT_CODER_H
#define ROCKET_ABSTRACT_CODER_H

#include <vector>
#include "rocket/net/tcp/tcp_buffer.h"
#include "abstract_protocol.h"

namespace rocket {

    class AbstractCoder {
    public:
        // 将message对象转换为字节流，写入buffer
        virtual void encode(std::vector<AbstractProtocol::s_ptr>& messages, TcpBuffer::s_ptr out_buffer) = 0;

        // 将buffer中的字节流转换为message
        virtual void decode(std::vector<AbstractProtocol::s_ptr>& out_messages, TcpBuffer::s_ptr buffer) = 0;

        virtual ~AbstractCoder() {}
    };

}

#endif //ROCKET_ABSTRACT_CODER_H
