#ifndef ROCKET_FD_EVENT_H
#define ROCKET_FD_EVENT_H

#include <sys/epoll.h>
#include <functional>

namespace rocket {
    // 封装事件
    class FdEvent {
    public:

        enum TriggerEvent {
            IN_EVENT = EPOLLIN, // 读事件
            OUT_EVENT = EPOLLOUT, // 写事件
            ERROR_EVENT = EPOLLERR, // ERR事件是默认添加到epollfd，不需要自己添加
        };

        FdEvent(int fd);

        FdEvent(); // 可能创建event的时候还没有创建套接字

        ~FdEvent();

        void setNonBlock();

        // 根据事件的类型返回对应的回调函数
        std::function<void()> handler(TriggerEvent event_type);
        // 根据事件类型绑定相应回调函数
        void listen(TriggerEvent event_type, std::function<void()> callback, std::function<void()> error_callback = nullptr);

        void cancel(TriggerEvent event_type); // 取消监听

        int getFd() const {
            return m_fd;
        }

        epoll_event getEpollEvent() {
            return m_listen_events;
        }

    protected:
        int m_fd {-1}; // 事件的文件描述符

        epoll_event m_listen_events; // 所监听的事件

        std::function<void()> m_read_callback {nullptr}; // 读回调函数
        std::function<void()> m_write_callback {nullptr}; // 写回调函数
        std::function<void()> m_error_callback {nullptr}; // 错误回调
    };
}

#endif //ROCKET_FD_EVENT_H
