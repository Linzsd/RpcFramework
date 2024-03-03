#include <iostream>
#include <unistd.h>
#include <memory>
#include <pthread.h>
#include "rocket/common/config.h"
#include "rocket/common/log.h"
#include "rocket/coroutine/coroutine.h"


rocket::Coroutine::s_ptr cor;
rocket::Coroutine::s_ptr cor2;

//void fun1() {
//    std::cout << "cor1 ---- now fitst resume fun1 coroutine by thread 1" << std::endl;
//    std::cout << "cor1 ---- now begin to yield fun1 coroutine" << std::endl;
//
//    test_.m_coroutine_mutex.lock();
//
//    std::cout << "cor1 ---- coroutine lock on test_, sleep 5s begin" << std::endl;
//
//    sleep(5);
//    std::cout << "cor1 ---- sleep 5s end, now back coroutine lock" << std::endl;
//
//    test_.m_coroutine_mutex.unlock();
//
//    tinyrpc::Coroutine::Yield();
//    std::cout << "cor1 ---- fun1 coroutine back, now end" << std::endl;
//
//}

//void fun2() {
//    std::cout << "cor222 ---- now fitst resume fun1 coroutine by thread 1" << std::endl;
//    std::cout << "cor222 ---- now begin to yield fun1 coroutine" << std::endl;
//
//    sleep(2);
//    std::cout << "cor222 ---- coroutine2 want to get coroutine lock of test_" << std::endl;
//    test_.m_coroutine_mutex.lock();
//
//    std::cout << "cor222 ---- coroutine2 get coroutine lock of test_ succ" << std::endl;
//
//}

void fun() {
    std::cout << "this is a sub co" << std::endl;

    rocket::Coroutine::Yield(); // 回到主协程

    std::cout << "sub co back" << std::endl;
}

void fun2() {
    std::cout << "this is another sub co" << std::endl;
    rocket::Coroutine::Yield(); // 回到主协程
}

int main(int argc, char* argv[]) {

    rocket::Config::SetGlobalConfig(nullptr);

    rocket::Logger::InitGlobalLogger(1);

    rocket::Coroutine::GetCurrentCoroutine();
    std::cout << "main begin" << std::endl;
    int stack_size = 128 * 1024;
    char* sp = reinterpret_cast<char*>(malloc(stack_size));
    cor = std::make_shared<rocket::Coroutine>(stack_size, sp, fun);

    std::cout << "this is main co" << std::endl;
    rocket::Coroutine::Resume(cor.get()); // 切换到子协程
    std::cout << "main co back 1" << std::endl;

    rocket::Coroutine::Resume(cor.get()); // 切换到子协程
    std::cout << "main co back 2" << std::endl;

    char* sp2 = reinterpret_cast<char*>(malloc(stack_size));
    cor2 = std::make_shared<rocket::Coroutine>(stack_size, sp2, fun2);
    rocket::Coroutine::Resume(cor2.get());

//    pthread_t thread2;
//    pthread_create(&thread2, NULL, &thread2_func, NULL);
//
//    thread1_func(NULL);
//
//    pthread_join(thread2, NULL);

    std::cout << "main end" << std::endl;
}