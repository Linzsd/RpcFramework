#include <vector>
#include "rocket/common/config.h"
#include "rocket/coroutine/coroutine_pool.h"


namespace rocket {

    // 每个线程独享一个协程池，1:N模型
    static thread_local CoroutinePool* t_coroutine_container_ptr = nullptr;

    CoroutinePool* CoroutinePool::GetCoroutinePool() {
        if (!t_coroutine_container_ptr) {
            t_coroutine_container_ptr = new CoroutinePool(1000, 128 * 1024); //TODO:将协程池大小与协程栈大小写入配置文件
        }
        return t_coroutine_container_ptr;
    }


    CoroutinePool::CoroutinePool(int pool_size, int stack_size /*= 1024 * 128*/) : m_pool_size(pool_size), m_stack_size(stack_size) {
        m_index = getCoroutineIndex();

        if (m_index == 0) {
            // skip main coroutine
            m_index = 1;
        }

        m_free_cors.resize(pool_size + m_index);
        for (int i = m_index; i < pool_size + m_index; ++i) {
            char* stack_sp = reinterpret_cast<char*>(malloc(m_stack_size));
            m_free_cors[i] = std::make_pair(std::make_shared<Coroutine>(stack_size, stack_sp), false);
        }

    }

    CoroutinePool::~CoroutinePool() {

    }

    Coroutine::s_ptr CoroutinePool::getCoroutineInstanse() {

        for (int i = m_index; i < m_pool_size; ++i) {
            if (!m_free_cors[i].first->getIsInCoFunc() && !m_free_cors[i].second) {
                m_free_cors[i].second = true;
                return m_free_cors[i].first;
            }
        }
        int newsize = (int)(1.5 * m_pool_size);
        for (int i = 0; i < newsize - m_pool_size; ++i) {
            char* stack_sp = reinterpret_cast<char*>(malloc(m_stack_size));
            m_free_cors.push_back(m_free_cors[i] = std::make_pair(std::make_shared<Coroutine>(m_stack_size, stack_sp), false));
        }
        int tmp = m_pool_size;
        m_pool_size = newsize;
        return m_free_cors[tmp].first;

    }

    void CoroutinePool::returnCoroutine(Coroutine::s_ptr cor) {
        m_free_cors[cor->getCorId()].second = false;
    }



}