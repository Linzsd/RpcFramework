#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <iostream>
#include "rocket/common/log.h"
#include "rocket/common/config.h"
#include "rocket/net/fd_event.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/timer_event.h"
#include "rocket/net/io_thread.h"
#include "rocket/net/io_thread_group.h"
#include "rocket/coroutine/coroutine.h"
#include "rocket/coroutine/coroutine_hook.h"

void test_io_thread() {
     int listenfd = socket(AF_INET, SOCK_STREAM, 0);
     if (listenfd == -1) {
       ERRORLOG("listenfd = -1");
       exit(0);
     }

     sockaddr_in addr;
     memset(&addr, 0, sizeof(addr));

     addr.sin_port = htons(12345);
     addr.sin_family = AF_INET;
     inet_aton("127.0.0.1", &addr.sin_addr);

     int rt = bind(listenfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
     if (rt != 0) {
       ERRORLOG("bind error");
       exit(1);
     }

     rt = listen(listenfd, 100);
     if (rt != 0) {
       ERRORLOG("listen error");
       exit(1);
     }

     rocket::FdEvent event(listenfd);
     event.listen(rocket::FdEvent::IN_EVENT, [listenfd](){
       sockaddr_in peer_addr;
       socklen_t addr_len = sizeof(peer_addr);
       memset(&peer_addr, 0, sizeof(peer_addr));
       int clientfd = accept(listenfd, reinterpret_cast<sockaddr*>(&peer_addr), &addr_len);

       DEBUGLOG("success get client fd[%d], peer addr: [%s:%d]", clientfd, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));

     });

    int i = 0;
    rocket::TimerEvent::s_ptr timer_event = std::make_shared<rocket::TimerEvent>(
        1000, true, [&i]() {
            INFOLOG("trigger timer event, count=%d", i++);
        }
    );

//    rocket::IOThread io_thread;
//
//    io_thread.getEventLoop()->addEpollEvent(&event);
//    io_thread.getEventLoop()->addTimerEvent(timer_event);
//    io_thread.start();
//
//    io_thread.join();

    rocket::IOThreadGroup io_thread_group = rocket::IOThreadGroup(2);
    rocket::IOThread* io_thread = io_thread_group.getIOThread();
    io_thread->getEventLoop()->addEpollEvent(&event);
    io_thread->getEventLoop()->addTimerEvent(timer_event);

    rocket::IOThread* io_thread2 = io_thread_group.getIOThread();
    io_thread2->getEventLoop()->addTimerEvent(timer_event);

    io_thread_group.start();
    io_thread_group.join();
}

void fun(int listenfd) {
    sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    memset(&peer_addr, 0, sizeof(peer_addr));
    DEBUGLOG("start accept");
    int clientfd = accept(listenfd, reinterpret_cast<sockaddr*>(&peer_addr), &addr_len);

    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(peer_addr.sin_addr), ipAddress, INET_ADDRSTRLEN);
    DEBUGLOG("fd[%d] has accepted, ip = %s", clientfd, ipAddress);
}

int main() {
    rocket::Config::SetGlobalConfig("../conf/rocket.xml");
    rocket::Logger::InitGlobalLogger(0);

//    test_io_thread();

    rocket::EventLoop* eventLoop = new rocket::EventLoop();

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        ERRORLOG("listenfd = -1");
        exit(0);
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(12346);
    addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &addr.sin_addr);

    int rt = bind(listenfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rt != 0) {
        ERRORLOG("bind error");
        exit(1);
    }
    rt = listen(listenfd, 100);
    if (rt != 0) {
        ERRORLOG("listen error");
        exit(1);
    }
    rocket::FdEvent event(listenfd);
    event.setNonBlock();
//    event.listen(rocket::FdEvent::IN_EVENT, [listenfd]() {
//        sockaddr_in peer_addr;
//        socklen_t addr_len = sizeof(peer_addr);
//        memset(&peer_addr, 0, sizeof(peer_addr));
//        int clientfd = accept(listenfd, reinterpret_cast<sockaddr*>(&peer_addr), &addr_len);
//        DEBUGLOG("success get client fd[%d], peer addr: [%s:%d]", clientfd, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
//    });
//    eventLoop->addEpollEvent(&event);
    // 协程
    rocket::Coroutine::GetCurrentCoroutine();
    DEBUGLOG("this is main co");

    int stack_size = 128 * 1024;
    char* sp = reinterpret_cast<char*>(malloc(stack_size));
    rocket::Coroutine::s_ptr cor = std::make_shared<rocket::Coroutine>(stack_size, sp, std::bind(fun, listenfd));
    rocket::Coroutine::Resume(cor.get());

    int i = 0;
    rocket::TimerEvent::s_ptr timer_event = std::make_shared<rocket::TimerEvent>(1000, true, [&i](){
        INFOLOG("trigger timer event, count=%d", i++);
    });

    eventLoop->addTimerEvent(timer_event);

    eventLoop->loop();
    return 0;
}
