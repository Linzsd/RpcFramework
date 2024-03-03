#ifndef ROCKET_EVENTLOOP_H
#define ROCKET_EVENTLOOP_H

#include <pthread.h>
#include <set>
#include <functional>
#include <queue>
#include "rocket/common/mutex.h"
#include "rocket/net/fd_event.h"
#include "rocket/net/wakeup_fd_event.h"
#include "rocket/net/timer.h"
#include "rocket/coroutine/coroutine.h"

namespace rocket {
    class EventLoop {
    public:
        EventLoop();

        ~EventLoop();

        void loop(); // 核心

        void wakeup(); // 将线程从epoll_wait唤醒

        void stop();

        void addEpollEvent(FdEvent* event);

        void deleteEpollEvent(FdEvent* event);

        bool isInLoopThread(); // 判断是否在当前线程

        void addTask(std::function<void()> callback, bool is_wake_up = false); // 将任务添加到队列中，等线程从epollwait返回后，由该线程执行这些任务，而不是其他线程

        void addTimerEvent(TimerEvent::s_ptr event);

        void addCoroutine(rocket::Coroutine::s_ptr, bool is_wake_up = true);

        Timer* getTimer();

        bool isLooping();

    public:
        static EventLoop* GetCurrentEventLoop();

    private:
        void dealWakeup();

        void initWakeUpFdEvent();

        void initTimer();


    private:
        pid_t m_thread_id {0}; // 记录线程号，因为每个线程只能有一个eventloop

        int m_epoll_fd {0};
        int m_wakeup_fd {0}; // 用于唤醒的套接字
        WakeUpFdEvent* m_wakeup_fd_event {nullptr};

        bool m_stop_flag {false}; // loop的停止标志

        std::set<int> m_listen_fds; // 当前eventloop监听的所有套接字

        std::queue<std::function<void()>> m_pending_tasks; // 所有待执行的任务队列

        Mutex m_mutex;

        Timer* m_timer {nullptr};

        bool m_is_looping {false};
    };
}

#endif //ROCKET_EVENTLOOP_H
