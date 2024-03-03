#include <unistd.h>
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/common/log.h"
#include "rocket/net/coder/string_coder.h"
#include "rocket/net/coder/tinypb_coder.h"
#include "rocket/coroutine/coroutine_pool.h"


namespace rocket {

    TcpConnection::TcpConnection(EventLoop* event_loop, int fd, int buffer_size, NetAddr::s_ptr peer_addr,
                                 NetAddr::s_ptr local_addr, TcpConnectionType type /*= TcpConnectionByServer*/)
        : m_event_loop(event_loop), m_peer_addr(peer_addr), m_local_addr(local_addr),
        m_fd(fd), m_connection_type(type) {

        if (m_connection_type == TcpConnectionByServer) {
            m_state = Connected;
        } else {
            m_state = NotConnected;
        }

        m_in_buffer = std::make_shared<TcpBuffer>(buffer_size);
        m_out_buffer = std::make_shared<TcpBuffer>(buffer_size);

        m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(m_fd);
        m_fd_event->setNonBlock();
        m_coder = new TinyPBCoder();

        if (m_connection_type == TcpConnectionByServer) {
//            listenRead();
            m_loop_cor = CoroutinePool::GetCoroutinePool()->getCoroutineInstanse();
            m_loop_cor->setCallBack(std::bind(&TcpConnection::MainServerLoopCorFunc, this));
            m_event_loop->addCoroutine(m_loop_cor);
        }
    }

    TcpConnection::~TcpConnection() {
        DEBUGLOG("~TcpConnection");
        if (m_coder) {
            delete m_coder;
            m_coder = nullptr;
        }
    }

    void TcpConnection::MainServerLoopCorFunc() {

        while (!m_stop) {
            DEBUGLOG("MainServerLoopCorFunc onRead")
            onRead();
            DEBUGLOG("MainServerLoopCorFunc excute")
            excute();
            DEBUGLOG("MainServerLoopCorFunc onWrite")
            onWrite();
        }
        INFOLOG("this connection has already end loop");
    }

    void TcpConnection::onRead() {
        // 1. 从 socket 缓冲区，调用系统的 read 函数读取字节 in_buffer 里面
        if (m_state != Connected) {
            ERRORLOG("onRead error, client has already disconnected, addr[%s], client_fd[%d]", m_peer_addr->toString().c_str(), m_fd);
            return;
        }
        bool is_read_all = false;
        bool is_close = false;
        while (!is_read_all) {
            // 扩容
            if (m_in_buffer->writeAble() == 0) {
                m_in_buffer->resizeBuffer(2 * m_in_buffer->m_buffer.size());
            }
            int read_count = m_in_buffer->writeAble();
            int write_index = m_in_buffer->writeIndex();
            // 写入m_in_buffer
            int rt = read(m_fd, &(m_in_buffer->m_buffer[write_index]), read_count);
            DEBUGLOG("success read [%d] bytes from addr [%s], client_fd[%d]", rt, m_peer_addr->toString().c_str(), m_fd);
            if (rt > 0) {
                m_in_buffer->moveWriteIndex(rt);
                if (rt == read_count) {
                    continue; // 缓冲区数据可能还没读完
                } else if (rt < read_count) {
                    // 缓冲区数据已经读完
                    is_read_all = true;
                    break;
                }
            } else if (rt == 0) {
                // 触发可读事件，但读出来小于等于0，说明对端关闭
                is_close = true;
                break;
            } else if (rt == -1 && errno == EAGAIN) {
                is_read_all = true;
                break;
            }
        }
        if (is_close) {
            INFOLOG("peer closed, peer addr=[%s], client_fd=[%d]", m_peer_addr->toString().c_str(), m_fd);
            clear();
            return;
        }
        if (!is_read_all) {
            ERRORLOG("not read all data");
        }
    }

    void TcpConnection::excute() {
        if (m_connection_type == TcpConnectionByServer) {
            // 将 rpc 请求执行业务逻辑，获取 rpc 响应，再把 rpc 响应发送回去
            std::vector<AbstractProtocol::s_ptr> result;
            std::vector<AbstractProtocol::s_ptr> replay_messages;

            m_coder->decode(result, m_in_buffer);
            for (int i = 0; i < result.size(); ++i) {
                // 1. 针对每个请求，调用rpc方法，获取响应 message
                // 2. 将响应 message 放入到发送缓冲区，监听可写事件回包
                INFOLOG("success get request[%s] from client=[%s]", result[i]->m_msg_id.c_str(), m_peer_addr->toString().c_str());
                std::shared_ptr<TinyPBProtocol> message = std::make_shared<TinyPBProtocol>();
//                message->m_pb_data = "hello, this is rocket rpc test";
//                message->m_msg_id = result[i]->m_msg_id;

                RpcDispatcher::GetRpcDispatcher()->dispatch(result[i], message, this);
                replay_messages.emplace_back(message);
            }
            m_coder->encode(replay_messages, m_out_buffer);

        } else {
            // 从 buffer 里 decode 得到 protocol
            std::vector<AbstractProtocol::s_ptr> result;
            m_coder->decode(result, m_in_buffer);
            for (int i = 0; i < result.size(); ++i) {
                std::string msg_id = result[i]->m_msg_id;
                m_reply_msg.insert(std::make_pair(msg_id, std::dynamic_pointer_cast<TinyPBProtocol>(result[i])));
            }
        }
    }

    void TcpConnection::onWrite() {
        if (m_is_over_time) {
            return;
        }
        // 将当前 out_buffer 里面的数据全部发送给 client
        if (m_state != Connected) {
            ERRORLOG("onWrite error, client has already disconnected, addr[%s], client_fd[%d]", m_peer_addr->toString().c_str(), m_fd);
            return;
        }

        // 发送数据
        bool is_write_all = false;
        while (true) {
            if (m_out_buffer->readAble() == 0) {
                DEBUGLOG("no data need to send to client [%s]", m_peer_addr->toString().c_str());
                is_write_all = true;
                break;
            }
            int write_size = m_out_buffer->readAble();
            int read_index = m_out_buffer->readIndex();

            int rt = write(m_fd, &(m_out_buffer->m_buffer[read_index]), write_size);
            if (rt >= write_size) {
                DEBUGLOG("no data need to send to client [%s]", m_peer_addr->toString().c_str());
                is_write_all = true;
                break;
            }
            if (rt == -1 && errno == EAGAIN){
                // 发送缓冲区已经满了，不能再发送，这种情况直接等下次fd可写的时候再次发送数据即可
                ERRORLOG("write data error, error==EAGAIN and rt == -1");
                break;
            }

            if (m_is_over_time) {
                INFOLOG("over timer, now break write function");
                break;
            }
        }
        if (is_write_all) {
//            m_fd_event->cancel(FdEvent::OUT_EVENT);
//            m_event_loop->addEpollEvent(m_fd_event);
            INFOLOG("send all data, now unregister write event and break");
        }
    }

    void TcpConnection::setState(const TcpState state) {
        m_state = state;
    }

    TcpState TcpConnection::getState() {
        return m_state;
    }

    void TcpConnection::clear() {
        // 处理一些关闭连接后的清理动作
        if (m_state == Closed) {
            return;
        }
        m_fd_event->cancel(FdEvent::IN_EVENT);
        m_fd_event->cancel(FdEvent::OUT_EVENT);
        m_event_loop->deleteEpollEvent(m_fd_event);

        m_stop = true;
        m_state = Closed;
    }

    void TcpConnection::shutdown() {
        if (m_state == Closed || m_state == NotConnected) {
            return;
        }
        m_state = HalfClosing; // 处于半关闭状态
        // 服务器不会对这个fd进行读写操作了，但客户端还可以读写
        // 相关知识：tcp四次挥手
        // 发送 FIN 报文，触发四次挥手第一阶段
        // 当 fd 发生可读事件，但可读数据为0，即对端发送了 FIN 报文，服务器处于time_wait
        ::shutdown(m_fd, SHUT_RDWR);
    }
    // 获取response并存入 TinyPBProtocol::s_ptr& pb_protocol，注意是引用传递
    bool TcpConnection::getResPackageData(const std::string& msg_id, TinyPBProtocol::s_ptr& pb_protocol) {
        auto it = m_reply_msg.find(msg_id);
        if (it != m_reply_msg.end()) {
            DEBUGLOG("return a resdata");
            pb_protocol = it->second;
            m_reply_msg.erase(it);
            return true;
        }
        DEBUGLOG("%s | reply data not exist", msg_id.c_str());
        return false;

    }

    void TcpConnection::setConnectionType(TcpConnectionType type) {
        m_connection_type = type;
    }

    void TcpConnection::listenWrite() {
        m_fd_event->listen(FdEvent::OUT_EVENT, std::bind(&TcpConnection::onWrite, this));
        m_event_loop->addEpollEvent(m_fd_event);
    }

    void TcpConnection::listenRead() {
        m_fd_event->listen(FdEvent::IN_EVENT, std::bind(&TcpConnection::onRead, this));
        m_event_loop->addEpollEvent(m_fd_event);
    }


    NetAddr::s_ptr TcpConnection::getLocalAddr() {
        return m_local_addr;
    }

    NetAddr::s_ptr TcpConnection::getPeerAddr() {
        return m_peer_addr;
    }

    void TcpConnection::setOverTimeFlag(bool value) {
        m_is_over_time = value;
    }

    bool TcpConnection::getOverTimerFlag() {
        return m_is_over_time;
    }

    AbstractCoder* TcpConnection::getCoder() const {
        return m_coder;
    }

    TcpBuffer::s_ptr TcpConnection::getInBuffer() {
        return m_in_buffer;
    }

    TcpBuffer::s_ptr TcpConnection::getOutBuffer() {
        return m_out_buffer;
    }
}
