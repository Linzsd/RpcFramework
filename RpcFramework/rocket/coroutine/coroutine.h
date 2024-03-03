#ifndef ROCKET_COROUTINE_H
#define ROCKET_COROUTINE_H

#include <functional>
#include <memory>
#include "rocket/coroutine/coctx.h"
#include "rocket/common/run_time.h"

namespace rocket {

    int getCoroutineIndex();

    class Coroutine {
    public:
        typedef std::shared_ptr<Coroutine> s_ptr;

    private:
        // 主协程构造函数
        Coroutine();

    public:
        // 用户协程构造函数
        Coroutine(int size, char* stack_ptr);

        Coroutine(int size, char* stack_ptr, std::function<void()> cb);

        ~Coroutine();
        // 设置回调函数
        bool setCallBack(std::function<void()> cb);

        int getCorId() const {
            return m_cor_id;
        }

        void setIsInCoFunc(const bool v) {
            m_is_in_cofunc = v;
        }

        bool getIsInCoFunc() const {
            return m_is_in_cofunc;
        }

        std::string getMsgNo() {
            return m_msg_no;
        }

        RunTime* getRunTime() {
            return &m_run_time;
        }

        void setMsgNo(const std::string& msg_no) {
            m_msg_no = msg_no;
        }

        void setIndex(int index) {
            m_index = index;
        }

        int getIndex() {
            return m_index;
        }

        char* getStackPtr() {
            return m_stack_sp;
        }

        int getStackSize() {
            return m_stack_size;
        }

        void setCanResume(bool v) {
            m_can_resume = v;
        }

    public:
        // 协程原语
        static void Yield(); // 挂起

        static void Resume(Coroutine* cor); // 唤醒

        static Coroutine* GetCurrentCoroutine();

        static Coroutine* GetMainCoroutine();

        static bool IsMainCoroutine();

    private:
        int m_cor_id {0}; // 协程id
        coctx m_coctx; // 协程寄存器上下文
        int m_stack_size {0}; // 协程申请堆空间的栈大小，单位：字节
        char* m_stack_sp {nullptr}; // 协程栈空间指针, 用malloc或mmap初始化该值
        bool m_is_in_cofunc {false};  // 调用协程的时候为true，协程结束时为false
        // RPC相关
        std::string m_msg_no;
        RunTime m_run_time;

        bool m_can_resume {true};

        int m_index {-1}; // 在协程池中的index

    public:
        std::function<void()> m_call_back; // 协程的回调函数
    };

}


#endif //ROCKET_COROUTINE_H
