#ifndef ROCKET_COCTX_H
#define ROCKET_COCTX_H

namespace rocket {

    enum {
        kRBP = 6,   // rbp, 栈底
        kRDI = 7,   // rdi, 函数的第一个参数
        kRSI = 8,   // rsi, 函数的第二个参数
        kRETAddr = 9,   // 下一条命令执行地址
        kRSP = 13,   // rsp, 栈顶
    };

    struct coctx {
        void* regs[14];
    };

    extern "C" {
        extern void coctx_swap(coctx *, coctx *) asm("coctx_swap");
    };
}

#endif //ROCKET_COCTX_H
