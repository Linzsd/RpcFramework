#ifndef ROCKET_TCP_SERVER_H
#define ROCKET_TCP_SERVER_H

#include <set>
#include "rocket/net/tcp/tcp_acceptor.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/io_thread_group.h"
#include "rocket/coroutine/coroutine.h"
#include "rocket/coroutine/coroutine_hook.h"
#include "rocket/coroutine/coroutine_pool.h"
#include "rocket/net/rpc/rpc_dispatcher.h"

namespace rocket {

    class CoroutinePool;

    // 是全局的单例对象，只能在主线程构建
    class TcpServer {
    public:
        TcpServer(NetAddr::s_ptr local_addr);

        TcpServer();

        ~TcpServer();

        void start();

        void addCoroutine(rocket::Coroutine::s_ptr cor);

        void updateService(const std::string& service_name, std::vector<std::string> addr);

        std::vector<std::string> getServiceAllAddr(const std::string& service_name);

        std::string getServiceWithLoadBalance(const std::string& service_name);

    public:
        static TcpServer* GetTcpServer();

        static void SetTcpServer(NetAddr::s_ptr local_addr);

    private:
        void init();

        void onAccept(); // 当有新客户端连接后需要执行

    private:
        TcpAcceptor::s_ptr m_acceptor;

        NetAddr::s_ptr m_local_addr; // 本地监听地址

        EventLoop* m_main_event_loop {nullptr}; // main reactor

        IOThreadGroup* m_io_thread_group {nullptr}; // sub reactor组

        Coroutine::s_ptr m_accept_cor;

        FdEvent* m_listen_fd_event;

        int m_client_counts {0}; // 连接数

        std::unordered_map<std::string, std::vector<std::string>> m_service_cache; // 从注册中心拉取的服务对应的IP:Port
    };

}


#endif //ROCKET_TCP_SERVER_H
