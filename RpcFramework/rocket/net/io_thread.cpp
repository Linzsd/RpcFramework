#include <assert.h>
#include "rocket/net/io_thread.h"
#include "rocket/common/log.h"
#include "rocket/common/util.h"
#include "rocket/coroutine/coroutine_pool.h"

namespace rocket {

    IOThread::IOThread() {

        int rt = sem_init(&m_init_semaphore, 0, 0);
        assert(rt == 0);

        rt = sem_init(&m_start_semaphore, 0, 0);
        assert(rt == 0);

        // 传this是为了后续方便取出该对象
        pthread_create(&m_thread, nullptr, &IOThread::Main, this);

        // wait, 直到新线程执行完 Main 函数的前置(即loop之前)
        // 多线程情况下，主线程得到子线程对象，若还没初始化就访问可能出错
        sem_wait(&m_init_semaphore);
        DEBUGLOG("IOThread [%d] create success", m_thread_id);
    }

    IOThread::~IOThread() {

        m_event_loop->stop();
        sem_destroy(&m_init_semaphore);
        sem_destroy(&m_start_semaphore);

        pthread_join(m_thread, nullptr); // 等待io线程结束

        if (m_event_loop) {
            delete m_event_loop;
            m_event_loop = nullptr;
        }
    }

    void *IOThread::Main(void *arg) {
        IOThread* thread = static_cast<IOThread*> (arg);

        thread->m_event_loop = new EventLoop();
        thread->m_thread_id = getThreadId();

        // 唤醒等待的线程
        sem_post(&thread->m_init_semaphore);

        // 让IO线程等待，直到我们主动启动
        DEBUGLOG("IOThread [%d] created, wait start semaphore", thread->m_thread_id);
        sem_wait(&thread->m_start_semaphore);

        // 创建一个主协程
        Coroutine::GetCurrentCoroutine();

        DEBUGLOG("IOThread [%d] start loop ", thread->m_thread_id);
        thread->m_event_loop->loop();
        DEBUGLOG("IOThread [%d] end loop ", thread->m_thread_id);

        return nullptr;
    }

    EventLoop *IOThread::getEventLoop() {
        return m_event_loop;
    }

    void IOThread::start() {
        DEBUGLOG("Now invoke IOThread [%d]", m_thread_id);
        sem_post(&m_start_semaphore);
    }

    void IOThread::join() {
        pthread_join(m_thread, nullptr);
    }

    void IOThread::addClient(int fd, NetAddr::s_ptr peer_addr, NetAddr::s_ptr local_addr) {
        TcpConnection::s_ptr conn = std::make_shared<TcpConnection>(m_event_loop, fd,
                                                                    128, peer_addr, local_addr);
        m_clients.insert(conn);
    }

}