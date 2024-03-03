#ifndef ROCKET_IO_THREAD_H
#define ROCKET_IO_THREAD_H

#include <pthread.h>
#include <semaphore.h>
#include "rocket/net/eventloop.h"
#include "rocket/coroutine/coroutine_pool.h"
#include "rocket/net/tcp/tcp_connection.h"

namespace rocket {

    class TcpConnection; // 解决循环依赖

    class IOThread {
    public:
        // 在构造函数创造线程，并在此线程创建eventloop
        IOThread();

        ~IOThread();

        EventLoop* getEventLoop();

        void start();

        void join();

        void addClient(int fd, NetAddr::s_ptr peer_addr, NetAddr::s_ptr local_addr);

    public:
        // 代码风格：静态函数开头大写
        static void* Main(void* arg);

    private:
        pid_t m_thread_id {0}; // 线程号
        pthread_t m_thread {0}; // 线程句柄

        EventLoop* m_event_loop {nullptr}; // 当前IO线程的loop对象
        std::set<std::shared_ptr<TcpConnection>> m_clients;

        sem_t m_init_semaphore;
        sem_t m_start_semaphore;
    };

}


#endif //ROCKET_IO_THREAD_H
