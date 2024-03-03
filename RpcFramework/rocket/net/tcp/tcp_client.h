#ifndef ROCKET_TCP_CLIENT_H
#define ROCKET_TCP_CLIENT_H

#include <memory>
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/timer_event.h"

namespace rocket {

    class TcpClient {
    public:
        typedef std::shared_ptr<TcpClient> s_ptr;

        TcpClient(NetAddr::s_ptr peer_addr);

        ~TcpClient();

        // 异步的进行 connect
        // 如果 connect 成功，done会被执行
        int sendAndRecvTinyPb(const std::string& msg_id, TinyPBProtocol::s_ptr& res);

        // 异步发送 message
        // 如果发送 message 成功，会调用done函数，入参是message对象
        void writeMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done);

        // 异步读取 message
        // 如果读取 message 成功，会调用done函数，入参是message对象
        void readMessage(const std::string& msg_id, std::function<void(AbstractProtocol::s_ptr)> done);

        void stop();

        int getConnectErrorCode();

        std::string getConnectErrorInfo();

        NetAddr::s_ptr getPeerAddr();

        NetAddr::s_ptr getLocalAddr();

        void initLocalAddr();

        void addTimerEvent(TimerEvent::s_ptr timer_event);

        TcpConnection::s_ptr getConnection();

    private:
        NetAddr::s_ptr m_peer_addr;
        NetAddr::s_ptr m_local_addr;

        EventLoop* m_event_loop {nullptr};

        int m_fd {-1};
        FdEvent* m_fd_event {nullptr};

        TcpConnection::s_ptr m_connection;

        int m_connect_error_code {0};
        std::string m_connect_error_info;
    };

}


#endif //ROCKET_TCP_CLIENT_H
