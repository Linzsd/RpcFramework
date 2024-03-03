#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include "rocket/net/coder/tinypb_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/common/util.h"
#include "rocket/common/log.h"

namespace rocket {

    void TinyPBCoder::encode(std::vector<AbstractProtocol::s_ptr> &messages, TcpBuffer::s_ptr out_buffer) {
        for (auto &i : messages){
            std::shared_ptr<TinyPBProtocol> msg = std::dynamic_pointer_cast<TinyPBProtocol>(i);
            int len =0;
            const char* buf = encodeTinyPB(msg, len);
            if (buf != nullptr && len != 0) {
                out_buffer->writeToBuffer(buf, len);
            }
            if (buf) {
                free((void*)buf);
                buf = nullptr;
            }
        }

    }

    void TinyPBCoder::decode(std::vector<AbstractProtocol::s_ptr> &out_messages, TcpBuffer::s_ptr buffer) {
        while(1) {
            // 遍历buffer，找到PB_START，找到后解析出整包长度，然后得到结束符位置，判断是否为PB_END
            std::vector<char> temp = buffer->m_buffer;
            int start_index = buffer->readIndex();
            int end_index = -1;
            int pk_len = 0; // 包长度
            bool parse_success = false;
            int i = 0;
            for (i = start_index; i < buffer->writeIndex(); ++i) {
                if (temp[i] == TinyPBProtocol::PB_START) {
                    // 从下取4个字节，由于是网络字节序，需要转换为主机字节序
                    if (i + 1 < buffer->writeIndex()) {
                        pk_len = getInt32FromNetByte(&temp[i+1]);
                        DEBUGLOG("get pk_len = %d", pk_len);
                        // 结束符索引
                        int j = i + pk_len - 1;
                        // 说明没读到整个包，跳过，放到下次读
                        if (j >= buffer->writeIndex()) {
                            continue;
                        }
                        if (temp[j] == TinyPBProtocol::PB_END) {
                            start_index = i;
                            end_index = j;
                            parse_success = true;
                            break;
                        }
                    }
                }
            }
            if (i >= buffer->writeIndex()) {
                DEBUGLOG("decode end, read all buffer data");
                return;
            }
            if (parse_success) {
                buffer->moveReadIndex(end_index - start_index + 1);
                std::shared_ptr<TinyPBProtocol> message = std::make_shared<TinyPBProtocol>();

                message->m_pk_len = pk_len;

                int msg_id_len_index = start_index + sizeof(char) + sizeof(message->m_pk_len);
                if (msg_id_len_index >= end_index) {
                    message->parse_success = false;
                    ERRORLOG("parse error, msg_id_len_index[%d] > end_index[%d]", msg_id_len_index, end_index);
                    continue;
                }
                message->m_msg_id_len = getInt32FromNetByte(&temp[msg_id_len_index]);
                DEBUGLOG("parse msg_id_len=%d", message->m_msg_id_len);

                int msg_id_index = msg_id_len_index + sizeof(message->m_msg_id_len);
                char msg_id[100] = {0};
                memcpy(&msg_id[0], &temp[msg_id_index], message->m_msg_id_len);
                message->m_msg_id = std::string(msg_id);
                DEBUGLOG("parse msg_id=%s", message->m_msg_id.c_str());

                int method_name_len_index = msg_id_index + message->m_msg_id_len;
                if (method_name_len_index >= end_index) {
                    message->parse_success = false;
                    ERRORLOG("parse error, method_name_len_index[%d] > end_index[%d]", method_name_len_index,
                             end_index);
                    continue;
                }
                message->m_method_name_len = getInt32FromNetByte(&temp[method_name_len_index]);
                DEBUGLOG("parse method_name_len=%d", message->m_method_name_len);

                int method_name_index = method_name_len_index + sizeof(message->m_method_name_len);
                char method_name[512] = {0};
                memcpy(&method_name[0], &temp[method_name_index], message->m_method_name_len);
                message->m_method_name = std::string(method_name);
                DEBUGLOG("parse method_name=%s", message->m_method_name.c_str());

                int err_code_index = method_name_index + message->m_method_name_len;
                if (err_code_index >= end_index) {
                    message->parse_success = false;
                    ERRORLOG("parse error, err_code_index[%d] > end_index[%d]", err_code_index, end_index);
                    continue;
                }
                message->m_err_code = getInt32FromNetByte(&temp[err_code_index]);
                DEBUGLOG("parse err_code=%d", message->m_err_code);

                int error_info_len_index = err_code_index + sizeof(message->m_err_code);
                if (error_info_len_index >= end_index) {
                    message->parse_success = false;
                    ERRORLOG("parse error, error_info_len_index[%d] > end_index[%d]", error_info_len_index, end_index);
                    continue;
                }
                message->m_err_info_len = getInt32FromNetByte(&temp[error_info_len_index]);
                DEBUGLOG("parse m_err_info_len=%d", message->m_err_info_len);

                int error_info_index = error_info_len_index + sizeof(message->m_err_info_len);
                char error_info[512] = {0};
                memcpy(&error_info[0], &temp[error_info_index], message->m_err_info_len);
                message->m_err_info = std::string(error_info);
                DEBUGLOG("parse error_info=%s", message->m_err_info.c_str());

                // 解析序列化数据
                int pb_data_len = message->m_pk_len - message->m_method_name_len - message->m_msg_id_len -
                                  message->m_err_info_len - 2 - 24;
                int pb_data_index = error_info_index + message->m_err_info_len;
                message->m_pb_data = std::string(&temp[pb_data_index], pb_data_len);

                // 校验和解析
                message->parse_success = true;
                out_messages.push_back(message);
            }
        }
    }

    const char *TinyPBCoder::encodeTinyPB(std::shared_ptr<TinyPBProtocol> message, int &len) {
        if (message->m_msg_id.empty()) {
            message->m_msg_id = "123456789";
        }
        DEBUGLOG("msg_id = %s", message->m_msg_id.c_str());

        int pk_len = 2 + 24 + message->m_msg_id.length() + message->m_method_name.length() +
                     message->m_err_info.length() + message->m_pb_data.length();
        DEBUGLOG("pk_len = %d", pk_len);

        char* buf = reinterpret_cast<char*>(malloc(pk_len));
        char* temp = buf;

        *temp = TinyPBProtocol::PB_START;
        temp ++;

        int32_t pk_len_net = htonl(pk_len);
        memcpy(temp, &pk_len_net, sizeof(pk_len_net));
        temp += sizeof(pk_len_net);

        int msg_id_len = message->m_msg_id.length();
        int32_t msg_id_len_net = htonl(msg_id_len);
        memcpy(temp, &msg_id_len_net, sizeof(msg_id_len_net));
        temp += sizeof(msg_id_len_net);

        if (!message->m_msg_id.empty()) {
            memcpy(temp, &(message->m_msg_id[0]), msg_id_len);
            temp += msg_id_len;
        }

        int method_name_len = message->m_method_name.length();
        int32_t method_name_len_net = htonl(method_name_len);
        memcpy(temp, &method_name_len_net, sizeof(method_name_len_net));
        temp += sizeof(method_name_len_net);

        if (!message->m_method_name.empty()) {
            memcpy(temp, &(message->m_method_name[0]), method_name_len);
            temp += method_name_len;
        }

        int32_t err_code_net = htonl(message->m_err_code);
        memcpy(temp, &err_code_net, sizeof(err_code_net));
        temp += sizeof(err_code_net);

        int err_info_len = message->m_err_info.length();
        int32_t err_info_len_net = htonl(err_info_len);
        memcpy(temp, &err_info_len_net, sizeof(err_info_len_net));
        temp += sizeof(err_info_len_net);

        if (!message->m_err_info.empty()) {
            memcpy(temp, &(message->m_err_info[0]), err_info_len);
            temp += err_info_len;
        }

        if (!message->m_pb_data.empty()) {
            memcpy(temp, &(message->m_pb_data[0]), message->m_pb_data.length());
            temp += message->m_pb_data.length();
        }

        int32_t check_sum_net = htonl(1);
        memcpy(temp, &check_sum_net, sizeof(check_sum_net));
        temp += sizeof(check_sum_net);

        *temp = TinyPBProtocol::PB_END;

        message->m_pk_len = pk_len;
        message->m_msg_id_len = msg_id_len;
        message->m_method_name_len = method_name_len;
        message->m_err_info_len = err_info_len;
        message->parse_success = true;
        len = pk_len;
        DEBUGLOG("encode message[%s] success", message->m_msg_id.c_str());

        return buf;
    }

}