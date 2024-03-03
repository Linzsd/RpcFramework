#include <dlfcn.h>
#include <cstring>
#include <unistd.h>
#include "rocket/coroutine/coroutine.h"
#include "rocket/coroutine/coroutine_hook.h"
#include "rocket/common/log.h"
#include "rocket/net/fd_event.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/net/eventloop.h"

/**
 * 展开后<name>_fun_ptr_t g_sys_<name>_fun = (<name>_fun_ptr_t)dlsym(RTLD_NEXT, "<name>");
 * 通过dlsym函数，RTLD_NEXT参数，意为寻找接下来的动态库中出现的第一个名为read的符号，赋值给g_sys_##name##_fun
 * g_sys_read_fun相当于系统调用的read
 **/
#define HOOK_SYS_FUNC(name) name##_fun_ptr_t g_sys_##name##_fun = (name##_fun_ptr_t)dlsym(RTLD_NEXT, #name);

HOOK_SYS_FUNC(accept);
HOOK_SYS_FUNC(read);
HOOK_SYS_FUNC(write);
HOOK_SYS_FUNC(connect);
HOOK_SYS_FUNC(sleep);

namespace rocket {

    static bool g_hook = true; // 是否启用hook

    void SetHook(bool value) {
        g_hook = value;
    }

    void toEpoll(rocket::EventLoop* loop, rocket::FdEvent* fd_event, int events) {

        rocket::Coroutine* cur_cor = rocket::Coroutine::GetCurrentCoroutine();
        if (events & rocket::FdEvent::TriggerEvent::IN_EVENT) {
            DEBUGLOG("fd:[%d], register read event to epoll", fd_event->getFd())

            fd_event->listen(rocket::FdEvent::TriggerEvent::IN_EVENT, [cur_cor]() {
                rocket::Coroutine::Resume(cur_cor);
            });

//            fd_event->setCoroutine(cur_cor);
//            fd_event->addListenEvents(tinyrpc::IOEvent::READ);
        }
        if (events & rocket::FdEvent::TriggerEvent::OUT_EVENT) {
            DEBUGLOG("fd[%d], register write event to epoll", fd_event->getFd());
            fd_event->listen(rocket::FdEvent::TriggerEvent::OUT_EVENT, [cur_cor]() {
                rocket::Coroutine::Resume(cur_cor);
            });
//            fd_event->setCoroutine(cur_cor);
//            fd_event->addListenEvents(tinyrpc::IOEvent::WRITE);
        }
        // fd_event->updateToReactor();
        loop->addEpollEvent(fd_event);
    }

    ssize_t read_hook(int fd, void *buf, size_t count) {
        DEBUGLOG("this is hook read");
        if (rocket::Coroutine::IsMainCoroutine()) {
            DEBUGLOG("hook disable, call sys read func");
            return g_sys_read_fun(fd, buf, count);
        }

        rocket::EventLoop* loop = rocket::EventLoop::GetCurrentEventLoop(); // 保证有eventloop
        // assert(reactor != nullptr);

        rocket::FdEvent* fd_event = rocket::FdEventGroup::GetFdEventGroup()->getFdEvent(fd);

        fd_event->setNonBlock();

        // must first register read event on epoll
        // because reactor should always care read event when a connection sockfd was created
        // so if first call sys read, and read return success, this fucntion will not register read event and return
        // for this connection sockfd, reactor will never care read event
        ssize_t n = g_sys_read_fun(fd, buf, count);
        if (n > 0) {
            return n;
        }

        toEpoll(loop, fd_event, rocket::FdEvent::TriggerEvent::IN_EVENT);

        DEBUGLOG("read func to yield");
        rocket::Coroutine::Yield();

        // 取消读监听
        fd_event->cancel(rocket::FdEvent::TriggerEvent::IN_EVENT);
        loop->addEpollEvent(fd_event);

//        fd_event->clearCoroutine();
        // fd_event->updateToReactor();

        DEBUGLOG("read func yield back, now to call sys read");
        return g_sys_read_fun(fd, buf, count);

    }

    int accept_hook(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
        DEBUGLOG("this is hook accept");
        if (rocket::Coroutine::IsMainCoroutine()) {
            DEBUGLOG("hook disable, call sys accept func");
            return g_sys_accept_fun(sockfd, addr, addrlen);
        }

        rocket::EventLoop* loop = rocket::EventLoop::GetCurrentEventLoop(); // 保证有eventloop
        // assert(reactor != nullptr);

        rocket::FdEvent* fd_event = rocket::FdEventGroup::GetFdEventGroup()->getFdEvent(sockfd);

        fd_event->setNonBlock();

        int n = g_sys_accept_fun(sockfd, addr, addrlen);
        if (n > 0) {
            return n;
        }

        toEpoll(loop, fd_event, rocket::FdEvent::TriggerEvent::IN_EVENT);

        DEBUGLOG("accept func to yield");
        rocket::Coroutine::Yield();

        // 取消读监听
        fd_event->cancel(rocket::FdEvent::TriggerEvent::IN_EVENT);
        loop->addEpollEvent(fd_event);

        DEBUGLOG("accept func yield back, now to call sys accept");
        return g_sys_accept_fun(sockfd, addr, addrlen);

    }

    ssize_t write_hook(int fd, const void *buf, size_t count) {
        DEBUGLOG("this is hook write");
        if (rocket::Coroutine::IsMainCoroutine()) {
            DEBUGLOG("hook disable, call sys write func");
            return g_sys_write_fun(fd, buf, count);
        }
        rocket::EventLoop* loop = rocket::EventLoop::GetCurrentEventLoop(); // 保证有eventloop
        // assert(reactor != nullptr);

        rocket::FdEvent* fd_event = rocket::FdEventGroup::GetFdEventGroup()->getFdEvent(fd);

        fd_event->setNonBlock();

        ssize_t n = g_sys_write_fun(fd, buf, count);
        if (n > 0) {
            return n;
        }

        toEpoll(loop, fd_event, rocket::FdEvent::TriggerEvent::OUT_EVENT);

        DEBUGLOG("write func to yield");
        rocket::Coroutine::Yield();

        // 取消写监听
        fd_event->cancel(rocket::FdEvent::TriggerEvent::OUT_EVENT);
        loop->addEpollEvent(fd_event);

        DEBUGLOG("write func yield back, now to call sys write");
        return g_sys_write_fun(fd, buf, count);
    }

    int connect_hook(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
        DEBUGLOG("this is hook connect");
        if (rocket::Coroutine::IsMainCoroutine()) {
            DEBUGLOG("hook disable, call sys connect func");
            return g_sys_connect_fun(sockfd, addr, addrlen);
        }
        rocket::EventLoop* loop = rocket::EventLoop::GetCurrentEventLoop();

        rocket::FdEvent* fd_event = rocket::FdEventGroup::GetFdEventGroup()->getFdEvent(sockfd);

        rocket::Coroutine* cur_cor = rocket::Coroutine::GetCurrentCoroutine();

        fd_event->setNonBlock();
        int n = g_sys_connect_fun(sockfd, addr, addrlen);
        if (n == 0) {
            DEBUGLOG("direct connect success, return");
            return n;
        } else if (errno != EINPROGRESS) {
            DEBUGLOG("connect error and errno is't EINPROGRESS, errno=%d,error=%s", errno, strerror(errno));
            return n;
        }

        DEBUGLOG("errno == EINPROGRESS");

        toEpoll(loop, fd_event, rocket::FdEvent::TriggerEvent::OUT_EVENT);

        bool is_timeout = false;		// 是否超时

        // 超时函数句柄
        auto timeout_cb = [&is_timeout, cur_cor](){
            // 设置超时标志，然后唤醒协程
            is_timeout = true;
            rocket::Coroutine::Resume(cur_cor);
        };

        rocket::TimerEvent::s_ptr event = std::make_shared<rocket::TimerEvent>(
                10000, false, timeout_cb);

        rocket::Timer* timer = loop->getTimer();
        timer->addTimerEvent(event);

        rocket::Coroutine::Yield();

        // write事件需要删除，因为连接成功后后面会重新监听该fd的写事件。
        fd_event->cancel(rocket::FdEvent::TriggerEvent::OUT_EVENT);
        loop->addEpollEvent(fd_event);

        // 定时器也需要删除
        timer->deleteTimerEvent(event);

        n = g_sys_connect_fun(sockfd, addr, addrlen);
        if ((n < 0 && errno == EISCONN) || n == 0) {
            DEBUGLOG("connect success");
            return 0;
        }

        if (is_timeout) {
            DEBUGLOG("connect error, timeout[%d ms]", Config::GetGlobalConfig()->m_log_sync_inteval);
            errno = ETIMEDOUT;
        }

        DEBUGLOG("connect error and errno=%d, error=%s", errno, strerror(errno));
        return -1;
    }
}

extern "C" {

    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
        if (!rocket::g_hook) {
            return g_sys_accept_fun(sockfd, addr, addrlen);
        } else {
            return rocket::accept_hook(sockfd, addr, addrlen);
        }
    }

    ssize_t read(int fd, void *buf, size_t count) {
        if (!rocket::g_hook) {
            return g_sys_read_fun(fd, buf, count);
        } else {
            return rocket::read_hook(fd, buf, count);
        }
    }

    ssize_t write(int fd, const void *buf, size_t count) {
        if (!rocket::g_hook) {
            return g_sys_write_fun(fd, buf, count);
        } else {
            return rocket::write_hook(fd, buf, count);
        }
    }

    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
        if (!rocket::g_hook) {
            return g_sys_connect_fun(sockfd, addr, addrlen);
        } else {
            return rocket::connect_hook(sockfd, addr, addrlen);
        }
    }


}