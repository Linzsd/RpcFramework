#include <random>
#include "rocket/net/tcp/tcp_server.h"
#include "rocket/common/log.h"
#include "rocket/common/config.h"


namespace rocket {

    static TcpServer* server = nullptr;

    TcpServer *TcpServer::GetTcpServer() {
        if (server != nullptr) {
            return server;
        }
        ERRORLOG("Server is not existed, you need create Server first!!")
        return nullptr;
    }

    void TcpServer::SetTcpServer(NetAddr::s_ptr local_addr) {
        server = new TcpServer(local_addr);
    }

    TcpServer::TcpServer(NetAddr::s_ptr local_addr) : m_local_addr(local_addr) {

        init();
        INFOLOG("rocket TcpServer listen success on [%s]", m_local_addr->toString().c_str());
    }

    TcpServer::~TcpServer() {
        if (m_main_event_loop) {
            delete m_main_event_loop;
            m_main_event_loop = nullptr;
        }
//        GetCoroutinePool()->returnCoroutine(m_accept_cor);
    }

    void TcpServer::start() {
        m_io_thread_group->start();
        m_main_event_loop->loop();
    }

    void TcpServer::init() {
        m_acceptor = std::make_shared<TcpAcceptor>(m_local_addr);

        m_main_event_loop = EventLoop::GetCurrentEventLoop();
        m_io_thread_group = new IOThreadGroup(Config::GetGlobalConfig()->m_io_threads);

//        m_listen_fd_event = new FdEvent(m_acceptor->getListenFd());
//        m_listen_fd_event->listen(FdEvent::IN_EVENT, std::bind(&TcpServer::onAccept, this));
//
//        m_main_event_loop->addEpollEvent(m_listen_fd_event);

        m_accept_cor = CoroutinePool::GetCoroutinePool()->getCoroutineInstanse();
        m_accept_cor->setCallBack(std::bind(&TcpServer::onAccept, this));
        rocket::Coroutine::Resume(m_accept_cor.get());
    }

    // 若改造成协程，需要写成死循环，不然协程退出了
    void TcpServer::onAccept() {
        while(1) {
            auto re = m_acceptor->toAccept();
            int client_fd = re.first;
            NetAddr::s_ptr peer_addr = re.second;
            m_client_counts++;

            //把client_fd添加到任意IO线程
            IOThread *io_thread = m_io_thread_group->getIOThread();
            /* 原来的模式有个bug，在主线程中make_shared<TcpConnection>，导致创建协程池(GetCoroutinePool())都在主线程中进行。
             * 因此所有的线程都使用了同一个协程池。
             * 这里把创建TcpConnection作为回调放入eventloop中进行，保证在子线程中创建TcpConnection，从而保证
             * 在子线程中创建协程池
             * */
            auto cb = [io_thread, client_fd, peer_addr, this](){
                io_thread->addClient(client_fd, peer_addr, this->m_local_addr);
            };
            io_thread->getEventLoop()->addTask(cb);
//            TcpConnection::s_ptr connection = std::make_shared<TcpConnection>(io_thread->getEventLoop(), client_fd, 128,
//                                                                              peer_addr, m_local_addr);
//            connection->setState(Connected);
//            m_clients.insert(connection);

            INFOLOG("TcpServer success get client, fd=%d", client_fd);
        }
    }

    void TcpServer::addCoroutine(rocket::Coroutine::s_ptr cor) {
        m_main_event_loop->addCoroutine(cor);
    }

    void TcpServer::updateService(const std::string& service_name, std::vector<std::string> addr) {
        m_service_cache[service_name] = addr;
    }

    std::vector<std::string> TcpServer::getServiceAllAddr(const std::string &service_name) {
        return m_service_cache[service_name];
    }

    std::string TcpServer::getServiceWithLoadBalance(const std::string &service_name) {

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, m_service_cache[service_name].size() - 1);
        int randomIndex = dis(gen);

        return m_service_cache[service_name][randomIndex];
    }

}
