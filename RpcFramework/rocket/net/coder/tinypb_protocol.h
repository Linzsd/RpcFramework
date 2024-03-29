#ifndef ROCKET_TINYPB_PROTOCOL_H
#define ROCKET_TINYPB_PROTOCOL_H

#include "rocket/net/coder/abstract_protocol.h"

namespace rocket {

    struct TinyPBProtocol : public AbstractProtocol {
    public:
        typedef std::shared_ptr<TinyPBProtocol> s_ptr;
        TinyPBProtocol() {}
        ~TinyPBProtocol() {}
    public:
        static char PB_START;
        static char PB_END;

    public:
        int32_t m_pk_len {0};
        int32_t m_msg_id_len {0};
        // msg_id 继承父类

        int32_t m_method_name_len {0}; // 方法名长度
        std::string m_method_name; // 方法名
        // 错误相关
        int32_t m_err_code {0}; // 错误码
        int32_t m_err_info_len {0}; // 错误信息长度
        std::string m_err_info; // 错误信息

        std::string m_pb_data; // 序列化数据
        int32_t m_check_sum {0}; // 校验码

        bool parse_success {false};
    };
}


#endif //ROCKET_TINYPB_PROTOCOL_H
