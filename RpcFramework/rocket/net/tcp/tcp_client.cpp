#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include "rocket/coroutine/coroutine_hook.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/common/log.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/common/error_code.h"
#include "rocket/net/tcp/net_addr.h"

namespace rocket {

    TcpClient::TcpClient(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr) {
        m_event_loop = EventLoop::GetCurrentEventLoop();
        m_fd = socket(peer_addr->getFamily(), SOCK_STREAM, 0);
        if (m_fd < 0) {
            ERRORLOG("TcpClient::TcpClient() error, failed to create fd");
            return;
        }

        m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(m_fd);
        m_fd_event->setNonBlock();

        m_connection = std::make_shared<TcpConnection>(m_event_loop, m_fd, 128, m_peer_addr,
                                                       nullptr, TcpConnectionByClient);
        m_connection->setConnectionType(TcpConnectionByClient);
    }

    TcpClient::~TcpClient() {
        DEBUGLOG("~TcpClient");
        if (m_fd > 0) {
            close(m_fd);
        }
    }

    int TcpClient::sendAndRecvTinyPb(const std::string& msg_id, TinyPBProtocol::s_ptr& res) {
        bool is_timeout = false;

        rocket::Coroutine* cur_cor = rocket::Coroutine::GetCurrentCoroutine();
        auto timer_cb = [this, &is_timeout, cur_cor]() {
            INFOLOG("TcpClient timer out event occur");
            is_timeout = true;
            m_connection->setOverTimeFlag(true);
            rocket::Coroutine::Resume(cur_cor);
        };
        TimerEvent::s_ptr event = std::make_shared<TimerEvent>(10000, false, timer_cb);
        m_event_loop->getTimer()->addTimerEvent(event);

        while (!is_timeout) {
            DEBUGLOG("begin to connect!");
            if (m_connection->getState() != TcpState::Connected) {
                int rt = connect(m_fd, m_peer_addr->getSockAddr(), m_peer_addr->getSockLen());
                if (rt == 0) {
                    DEBUGLOG("connect [%s] success!", m_peer_addr->toString().c_str());
                    initLocalAddr();
                    m_connection->setState(Connected);
                    break;
                }
                if (is_timeout) {
                    INFOLOG("connect timeout, break!");
                    goto err_deal;
                }
                if (errno == ECONNREFUSED) {
                    m_connect_error_code = ERROR_PEER_CLOSED;
                    m_connect_error_info = "connect refused, sys error = " + std::string(strerror(errno));
                    m_event_loop->getTimer()->deleteTimerEvent(event);
                    m_event_loop->deleteEpollEvent(m_fd_event);
                    close(m_fd);
                    m_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (m_fd < 0) {
                        ERRORLOG("TcpClient::TcpClient() error, failed to create fd");
                    }
                    return ERROR_PEER_CLOSED;
                }
            } else {
                break;
            }

        }

        if (m_connection->getState() != TcpState::Connected) {
            std::stringstream ss;
            ss << "connect peer addr[" << m_peer_addr->toString() << "] error. sys error=" << strerror(errno);
            m_connect_error_code = ERROR_FAILED_CONNECT;
            m_connect_error_info = ss.str();
            m_event_loop->getTimer()->deleteTimerEvent(event);
            return ERROR_FAILED_CONNECT;
        }

        m_connection->setState(Connected);
        m_connection->onWrite();
        if (m_connection->getOverTimerFlag()) {
            INFOLOG("send data over time");
            is_timeout = true;
            goto err_deal;
        }
        // 等待服务端处理完RPC请求，读取response
        while (!m_connection->getResPackageData(msg_id, res)) {
            DEBUGLOG("redo getResPackageData");
            m_connection->onRead();

            if (m_connection->getOverTimerFlag()) {
                INFOLOG("read data over time");
                is_timeout = true;
                goto err_deal;
            }
            if (m_connection->getState() == Closed) {
                INFOLOG("peer close");
                goto err_deal;
            }

            m_connection->excute();

        }

        m_event_loop->getTimer()->deleteTimerEvent(event);
        m_connect_error_info = "";
        return 0;

        err_deal:
        // connect error should close fd and reopen new one
        m_event_loop->deleteEpollEvent(m_fd_event);
        close(m_fd);
        m_fd = socket(AF_INET, SOCK_STREAM, 0);
        std::stringstream ss;
        if (is_timeout) {
            ss << "call rpc falied, over " << "10000" << " ms";
            m_connect_error_info = ss.str();
            m_connect_error_code = ERROR_RPC_CALL_TIMEOUT;

            m_connection->setOverTimeFlag(false);
            return ERROR_RPC_CALL_TIMEOUT;
        } else {
            ss << "call rpc falied, peer closed [" << m_peer_addr->toString() << "]";
            m_connect_error_info = ss.str();
            m_connect_error_code = ERROR_PEER_CLOSED;
            return ERROR_PEER_CLOSED;
        }

    }


    void TcpClient::stop() {
        if (m_event_loop->isLooping()) {
            m_event_loop->stop();
        }
    }

    int TcpClient::getConnectErrorCode() {
        return m_connect_error_code;
    }

    std::string TcpClient::getConnectErrorInfo() {
        return m_connect_error_info;
    }

    NetAddr::s_ptr TcpClient::getPeerAddr() {
        return m_peer_addr;
    }

    NetAddr::s_ptr TcpClient::getLocalAddr() {
        return m_local_addr;
    }

    void TcpClient::initLocalAddr() {
        sockaddr_in local_addr;
        socklen_t len = sizeof(local_addr);
        int ret = getsockname(m_fd, reinterpret_cast<sockaddr*>(&local_addr), &len);
        if (ret != 0) {
            ERRORLOG("initLocalAddr, getsockname error, errno=%d, error=%s", errno, strerror(errno));
            return;
        }

        m_local_addr = std::make_shared<IPNetAddr>(local_addr);
    }

    void TcpClient::addTimerEvent(TimerEvent::s_ptr timer_event) {
        m_event_loop->addTimerEvent(timer_event);
    }

    TcpConnection::s_ptr TcpClient::getConnection() {
        if (!m_connection.get()) {
            m_connection = std::make_shared<TcpConnection>(m_event_loop, m_fd, 128, m_peer_addr,
                                                           nullptr, TcpConnectionByClient);
        }
        return m_connection;
    }

}