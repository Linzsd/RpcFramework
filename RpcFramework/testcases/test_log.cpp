#include <pthread.h>
#include <unistd.h>
#include "rocket/common/log.h"
#include "rocket/common/config.h"

void* func(void*) {

    DEBUGLOG("debug this is thread in %s", "func")
    INFOLOG("info this is thread in %s", "func")

    return nullptr;
}

int main() {
    rocket::Config::SetGlobalConfig("../conf/rocket.xml");
    rocket::Logger::InitGlobalLogger();

    pthread_t thread;
    pthread_create(&thread, nullptr, &func, nullptr);
    sleep(1);
    DEBUGLOG("test debug log %s", "11");
    INFOLOG("test info log %s", "11");
    sleep(1);

    pthread_join(thread, nullptr);
    return 0;
}
