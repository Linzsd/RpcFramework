#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <cstring>
#include "rocket/net/eventloop.h"
#include "rocket/common/log.h"
#include "rocket/common/util.h"


#define ADD_TO_EPOLL() \
    auto it = m_listen_fds.find(event->getFd()); \
    int op = EPOLL_CTL_ADD; \
    if (it != m_listen_fds.end()) { \
        op = EPOLL_CTL_MOD; \
    } \
    epoll_event temp = event->getEpollEvent(); \
    int rt = epoll_ctl(m_epoll_fd, op, event->getFd(), &temp); \
    if (rt == -1) { \
        ERRORLOG("failed epoll_ctl when add fd, errno=%d, error info=%s", errno, strerror(errno)); \
    } \
    m_listen_fds.insert(event->getFd()); \
    DEBUGLOG("add event success, fd[%d]", event->getFd()); \

#define DELETE_FROM_EPOLL() \
    auto it = m_listen_fds.find(event->getFd()); \
    if (it == m_listen_fds.end()) { \
        return; \
    } \
    int op = EPOLL_CTL_DEL; \
    epoll_event temp = event->getEpollEvent(); \
    int rt = epoll_ctl(m_epoll_fd, op, event->getFd(), NULL); \
    if (rt == -1) { \
        ERRORLOG("failed epoll_ctl when delete fd, errno=%d, error info=%s", errno, strerror(errno)) \
    } \
    m_listen_fds.erase(event->getFd()); \
    DEBUGLOG("delete event success, fd[%d]", event->getFd()); \


namespace rocket {

    static thread_local EventLoop* t_current_eventloop = nullptr;
    static int g_epoll_max_timeout = 10000;
    static int g_epoll_max_events = 10;

    EventLoop::EventLoop() {
        // 判断当前线程是否已经有了eventloop
        if (t_current_eventloop != nullptr) {
            ERRORLOG("failed to create event loop, this thread has created event loop");
            exit(0);
        }
        m_thread_id = getThreadId();

        m_epoll_fd = epoll_create(10);
        if (m_epoll_fd == -1) {
            ERRORLOG("failed to create event loop, epoll_create, error info[%d]", errno);
            exit(0);
        }

        initWakeUpFdEvent();
        initTimer();

        INFOLOG("success create event loop in thread %d", m_thread_id);
        t_current_eventloop = this;
    }

    EventLoop::~EventLoop() {
        close(m_epoll_fd);
        if (m_wakeup_fd_event) {
            delete m_wakeup_fd_event;
            m_wakeup_fd_event = nullptr;
        }
        if (m_timer) {
            delete m_timer;
            m_timer = nullptr;
        }
    }

    void EventLoop::loop() {
        m_is_looping = true;

        while (!m_stop_flag) {
            ScopeMutex<Mutex> lock(m_mutex);
            std::queue<std::function<void()>> temp_tasks;
            m_pending_tasks.swap(temp_tasks);
            lock.unlock();
            while (!temp_tasks.empty()) {
                std::function<void()> cb = temp_tasks.front();
                temp_tasks.pop();
                if (cb) {
                    cb();
                }
            }

            // 如果有定时任务需要执行，在此处执行
            // 1、怎么判断一个定时任务需要执行？ (now_time > TimerEvent.arrive_time)
            // 2、怎么在arrive_time时间点从epoll_wait返回->如何让eventloop监听定时

            int timeout = g_epoll_max_timeout;
            epoll_event result_events[g_epoll_max_events];
//            DEBUGLOG("now begin to epoll_wait");
            int rt = epoll_wait(m_epoll_fd, result_events, g_epoll_max_events, timeout);
            DEBUGLOG("now end epoll_wait, rt = %d", rt);

            if (rt < 0) {
                ERRORLOG("epoll_wait error, errno=%d", errno);
            } else {
                for (int i = 0; i < rt; ++i) {
                    epoll_event trigger_event = result_events[i];
                    FdEvent* fd_event = static_cast<FdEvent*>(trigger_event.data.ptr);
                    if (fd_event == nullptr) {
                        ERRORLOG("fd_event = nullptr, continue");
                        continue;
                    }

                    if (trigger_event.events & EPOLLIN) {
                        DEBUGLOG("fd %d trigger EPOLLIN event", fd_event->getFd());
                        addTask(fd_event->handler(FdEvent::IN_EVENT));
                    }
                    if (trigger_event.events & EPOLLOUT) {
                        DEBUGLOG("fd %d trigger EPOLLOUT event", fd_event->getFd());
                        addTask(fd_event->handler(FdEvent::OUT_EVENT));
                    }
//                    if (!(trigger_event.events & EPOLLIN) && !(trigger_event.events & EPOLLOUT)){
//                        // 如果服务器未启动，会返回EPOLLHUP事件，若不处理则会死循环，一直从epollwait返回
//                        DEBUGLOG("unknown event = %d", (int)trigger_event.events);
//                    }
                    if (trigger_event.events & EPOLLERR) {
                        DEBUGLOG("fd %d trigger EPOLLERR event", fd_event->getFd());
                        deleteEpollEvent(fd_event);
                        if (fd_event->handler(FdEvent::ERROR_EVENT) != nullptr) {
                            addTask(fd_event->handler(FdEvent::OUT_EVENT));
                        }
                    }
                }
            }
        }
    }

    void EventLoop::initTimer() {
        m_timer = new Timer();
        addEpollEvent(m_timer);
    }

    Timer *EventLoop::getTimer() {
        return m_timer;
    }

    void EventLoop::addTimerEvent(TimerEvent::s_ptr event) {
        m_timer->addTimerEvent(event);
    }

    void EventLoop::initWakeUpFdEvent() {
        m_wakeup_fd = eventfd(0, EFD_NONBLOCK); // 非阻塞
        if (m_wakeup_fd < 0) {
            ERRORLOG("failed to create event loop, eventfd create error, error info[%d]", errno);
            exit(0);
        }

        m_wakeup_fd_event = new WakeUpFdEvent(m_wakeup_fd);
        DEBUGLOG("create wakeup fd[%d]", m_wakeup_fd)
        m_wakeup_fd_event->listen(FdEvent::IN_EVENT, [this]() {
            char buf[8];
            // 读完为止
            while(read(m_wakeup_fd, buf, 8) != -1 && errno != EAGAIN) {
            }
            DEBUGLOG("read full bytes from wakeup fd[%d]", m_wakeup_fd);
        });

        addEpollEvent(m_wakeup_fd_event);
    }

    void EventLoop::wakeup() {
        m_wakeup_fd_event->wakeup();
    }

    void EventLoop::stop() {
        m_stop_flag = true;
        wakeup();
    }

    void EventLoop::dealWakeup() {

    }

    void EventLoop::addEpollEvent(FdEvent *event) {
        if (isInLoopThread()) {
            ADD_TO_EPOLL();
        } else {
            // 如果不在该线程内，则将(添加到epoll监听)封装为回调函数，添加进任务队列
            auto cb = [this, event]() {
                ADD_TO_EPOLL();
            };
            addTask(cb, true);
        }
    }

    void EventLoop::deleteEpollEvent(FdEvent *event) {
        if (isInLoopThread()) {
            DELETE_FROM_EPOLL();
        } else {
            auto cb = [this, event]() {
                DELETE_FROM_EPOLL();
            };
            addTask(cb, true);
        }
    }

    bool EventLoop::isInLoopThread() {
        return getThreadId() == m_thread_id;
    }

    void EventLoop::addTask(std::function<void()> callback, bool is_wake_up /*=false*/) {
        ScopeMutex<Mutex> lock(m_mutex);
        m_pending_tasks.push(callback);
        lock.unlock();

        if (is_wake_up) {
            wakeup();
        }
    }

    void EventLoop::addCoroutine(rocket::Coroutine::s_ptr cor, bool is_wake_up /*=true*/) {
        auto func = [cor](){
            DEBUGLOG("Resume Server coroutine!!!")
            rocket::Coroutine::Resume(cor.get());
        };
        addTask(func, is_wake_up);
    }

    EventLoop *EventLoop::GetCurrentEventLoop() {
        if (t_current_eventloop) {
            return t_current_eventloop;
        }
        t_current_eventloop = new EventLoop();
        return t_current_eventloop;
    }

    bool EventLoop::isLooping() {
        return m_is_looping;
    }

}