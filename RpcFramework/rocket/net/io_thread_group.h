#ifndef ROCKET_IO_THREAD_GROUP_H
#define ROCKET_IO_THREAD_GROUP_H

#include <vector>
#include "rocket/common/log.h"
#include "rocket/net/io_thread.h"


namespace rocket {

    class IOThreadGroup {
    public:
        IOThreadGroup(int size);

        ~IOThreadGroup();

        void start(); // 控制所有IO线程的开始

        void join();

        IOThread* getIOThread(); // 获取可用的IO线程

    private:

        int m_size {0}; // 线程池大小
        std::vector<IOThread*> m_io_thread_groups;

        int m_index {0};
    };

}


#endif //ROCKET_IO_THREAD_GROUP_H
