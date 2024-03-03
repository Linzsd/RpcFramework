#ifndef ROCKET_TCP_CONNECTION_H
#define ROCKET_TCP_CONNECTION_H

#include <map>
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_buffer.h"
#include "rocket/net/io_thread.h"
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/coder/abstract_coder.h"
#include "rocket/net/rpc/rpc_dispatcher.h"

namespace rocket {

    enum TcpState {
        NotConnected = 1,
        Connected = 2,
        HalfClosing = 3,
        Closed = 4,
    };

    enum TcpConnectionType {
        TcpConnectionByServer = 1, // 作为服务端使用，代表跟客户端连接
        TcpConnectionByClient = 2, // 作为客户端使用，代表跟服务器连接
    };

    class TcpConnection {
    public:
        typedef std::shared_ptr<TcpConnection> s_ptr;
    public:
        TcpConnection(EventLoop* event_loop, int fd, int buffer_size, NetAddr::s_ptr peer_addr,
                      NetAddr::s_ptr local_addr, TcpConnectionType type = TcpConnectionByServer);

        ~TcpConnection();

        void MainServerLoopCorFunc();

        void onRead();

        void excute();

        void onWrite();

        void setState(const TcpState state);

        TcpState getState();

        void clear();

        void shutdown(); // 服务器主动关闭连接，处理恶意tcp连接(不做任何事情)

        bool getResPackageData(const std::string& msg_id, TinyPBProtocol::s_ptr& pb_protocol);

        void setConnectionType(TcpConnectionType type);

        void listenWrite(); // 启动监听可写事件

        void listenRead();

        void pushSendMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done);

        void pushReadMessage(const std::string& msg_id, std::function<void(AbstractProtocol::s_ptr)> done);

        NetAddr::s_ptr getLocalAddr();

        NetAddr::s_ptr getPeerAddr();

        void setOverTimeFlag(bool value);

        bool getOverTimerFlag();

        AbstractCoder* getCoder() const;

        TcpBuffer::s_ptr getInBuffer();

        TcpBuffer::s_ptr getOutBuffer();

    private:
        NetAddr::s_ptr m_local_addr; // 本地地址
        NetAddr::s_ptr m_peer_addr; // 客户端地址

        TcpBuffer::s_ptr m_in_buffer; // 接收缓冲区
        TcpBuffer::s_ptr m_out_buffer; // 发送缓冲区

        EventLoop* m_event_loop {nullptr};
        FdEvent* m_fd_event {nullptr};
        int m_fd {0};

        Coroutine::s_ptr m_loop_cor; // 读-执行-写循环协程

        AbstractCoder* m_coder {nullptr};

        TcpState m_state;

        bool m_stop {false};

        bool m_is_over_time {false};

        TcpConnectionType m_connection_type {TcpConnectionByServer};

//        std::vector<std::pair<AbstractProtocol::s_ptr, std::function<void(AbstractProtocol::s_ptr)>>> m_write_dones;

        // key为msg_id
        std::map<std::string, TinyPBProtocol::s_ptr> m_reply_msg;
    };

}


#endif //ROCKET_TCP_CONNECTION_H
