#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <atomic>
#include "rocket/coroutine/coroutine.h"
#include "rocket/common/log.h"


namespace rocket {

    // 主协程，每个io线程有一个主协程
    static thread_local Coroutine* t_main_coroutine = nullptr;

    // 当前线程在运行的协程
    static thread_local Coroutine* t_cur_coroutine = nullptr;

    static thread_local RunTime* t_cur_run_time = nullptr;

    static thread_local int t_coroutine_count = 0;

    static thread_local int t_cur_coroutine_id = 0;

//    static std::atomic_int t_coroutine_count {0};
//
//    static std::atomic_int t_cur_coroutine_id {0};

    int getCoroutineIndex() {
        return t_cur_coroutine_id;
    }

    RunTime* getCurrentRunTime() {
        return t_cur_run_time;
    }

    void setCurrentRunTime(RunTime* v) {
        t_cur_run_time = v;
    }

    void CoFunction(Coroutine* co) {

        if (co!= nullptr) {
            co->setIsInCoFunc(true);

            // 去执行协程回调函数
            co->m_call_back();

            co->setIsInCoFunc(false);
        }
        // 协程的回调函数执行完毕，需要切换回主协程，否则线程还处于这个子协程的上下文中，直接coredump
        Coroutine::Yield();
    }

    Coroutine::Coroutine() {
        // main coroutine'id is 0
        m_cor_id = t_cur_coroutine_id ++;
        t_coroutine_count++;
        memset(&m_coctx, 0, sizeof(m_coctx));
        t_cur_coroutine = this;
        DEBUGLOG("con | coroutine[%d] create", m_cor_id);
    }

    Coroutine::Coroutine(int size, char* stack_ptr) : m_stack_size(size), m_stack_sp(stack_ptr) {
        assert(stack_ptr);
        if (!t_main_coroutine) {
            t_main_coroutine = new Coroutine();
        }
        m_cor_id = t_cur_coroutine_id ++;
        t_coroutine_count ++;
        DEBUGLOG("stk | coroutine[%d] create", m_cor_id);
    }

    Coroutine::Coroutine(int size, char *stack_ptr, std::function<void()> cb) : m_stack_size(size), m_stack_sp(stack_ptr) {
        assert(m_stack_sp);

        if (!t_main_coroutine) {
            t_main_coroutine = new Coroutine();
        }

        setCallBack(cb);
        m_cor_id = t_cur_coroutine_id++;
        t_coroutine_count++;
        DEBUGLOG("cb | coroutine[%d] create", m_cor_id);
    }

    Coroutine::~Coroutine() {
        t_coroutine_count --;
        DEBUGLOG("coroutine[%d] die", m_cor_id);
    }

    bool Coroutine::setCallBack(std::function<void()> cb) {
        if (this == t_main_coroutine) {
            ERRORLOG("main coroutine can't set callback")
            return false;
        }
        if (m_is_in_cofunc) {
            ERRORLOG("this coroutine is in CoFunction");
            return false;
        }

        m_call_back = cb;

        char* top = m_stack_sp + m_stack_size;
        // first set 0 to stack
        // memset(&top, 0, m_stack_size);

        top = reinterpret_cast<char*>((reinterpret_cast<unsigned long>(top)) & -16LL); // 内存对齐8字节

        memset(&m_coctx, 0, sizeof(m_coctx));

        m_coctx.regs[kRSP] = top;
        m_coctx.regs[kRBP] = top;
        m_coctx.regs[kRETAddr] = reinterpret_cast<char*>(CoFunction);
        m_coctx.regs[kRDI] = reinterpret_cast<char*>(this);

        m_can_resume = true;

        return true;
    }

    /********
    从目标协程返回到主协程
    ********/
    void Coroutine::Yield() {
        if (t_main_coroutine == nullptr) {
            ERRORLOG("main coroutine is nullptr");
            return;
        }

        if (t_cur_coroutine == t_main_coroutine) {
            ERRORLOG("current coroutine is main coroutine");
            return;
        }
        Coroutine* co = t_cur_coroutine;
        t_cur_coroutine = t_main_coroutine; // 切换当前协程
        t_cur_run_time = nullptr;
        coctx_swap(&(co->m_coctx), &(t_main_coroutine->m_coctx));
        // DebugLog << "swap back";
        DEBUGLOG("Yield swap back");
    }

    /********
    从当前协程切换到目标协程
    ********/
    void Coroutine::Resume(Coroutine* co) {
        if (t_cur_coroutine != t_main_coroutine) {
            ERRORLOG("swap error, current coroutine must be main coroutine");
            return;
        }

        if (!t_main_coroutine) {
            ERRORLOG("main coroutine is nullptr");
            return;
        }
        if (!co || !co->m_can_resume) {
            ERRORLOG("pending coroutine is nullptr or can_resume is false");
            return;
        }

        if (t_cur_coroutine == co) {
            ERRORLOG("current coroutine is pending cor, need't swap");
            return;
        }

        t_cur_coroutine = co;
        t_cur_run_time = co->getRunTime();

        coctx_swap(&(t_main_coroutine->m_coctx), &(co->m_coctx));
        DEBUGLOG("Resume swap back");
    }

    Coroutine *Coroutine::GetCurrentCoroutine() {
        if (t_cur_coroutine == nullptr) {
            t_main_coroutine = new Coroutine();
            t_cur_coroutine = t_main_coroutine;
        }
        return t_cur_coroutine;
    }

    Coroutine *Coroutine::GetMainCoroutine() {
        if (t_main_coroutine) {
            return t_main_coroutine;
        }
        t_main_coroutine = new Coroutine();
        return t_main_coroutine;
    }

    bool Coroutine::IsMainCoroutine() {
        if (t_main_coroutine == nullptr || t_cur_coroutine == t_main_coroutine) {
            return true;
        }
        return false;
    }
}