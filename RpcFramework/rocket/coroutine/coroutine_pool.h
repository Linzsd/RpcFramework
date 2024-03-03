#ifndef ROCKET_COROUTINE_POOL_H
#define ROCKET_COROUTINE_POOL_H

#include <vector>
#include "rocket/coroutine/coroutine.h"

namespace rocket {

    class CoroutinePool {

    public:
        CoroutinePool(int pool_size, int stack_size = 1024 * 128);

        ~CoroutinePool();

        Coroutine::s_ptr getCoroutineInstanse();

        void returnCoroutine(Coroutine::s_ptr cor);

    public:
        static CoroutinePool* GetCoroutinePool();

    private:
        int m_index {0};
        int m_pool_size {0};
        int m_stack_size {0};

        // first--ptr of cor
        // second
        //    false -- can be dispatched
        //    true -- can't be dispatched
        std::vector<std::pair<Coroutine::s_ptr, bool>> m_free_cors;

    };



}

#endif //ROCKET_COROUTINE_POOL_H
