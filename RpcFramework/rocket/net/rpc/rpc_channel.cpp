#include <google/protobuf/service.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>

#include "rocket/net/rpc/rpc_channel.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/common/log.h"
#include "rocket/common/msg_util.h"
#include "rocket/common/error_code.h"
#include "rocket/net/timer_event.h"

namespace rocket {

    RpcChannel::RpcChannel(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr) {
        m_client = std::make_shared<TcpClient>(m_peer_addr);
    }

    RpcChannel::~RpcChannel() {
        INFOLOG("~RpcChannel");
    }

    void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                                google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                                google::protobuf::Message *response, google::protobuf::Closure *done) {

        std::shared_ptr<rocket::TinyPBProtocol> req_protocol = std::make_shared<rocket::TinyPBProtocol>();

        RpcController* my_controller = dynamic_cast<RpcController*>(controller);
        if (my_controller == nullptr) {
            ERRORLOG("failed callmethod, RpcController convert error");
            return;
        }
        my_controller->SetLocalAddr(m_client->getLocalAddr());
        my_controller->SetPeerAddr(m_client->getPeerAddr());

        if (my_controller->GetMsgId().empty()) {
            req_protocol->m_msg_id = MsgIDUtil::GenMsgID();
            my_controller->SetMsgId(req_protocol->m_msg_id);
        } else {
            req_protocol->m_msg_id = my_controller->GetMsgId();
        }

        req_protocol->m_method_name = method->full_name();
        INFOLOG("%s | call method name [%s]", req_protocol->m_msg_id.c_str(), req_protocol->m_method_name.c_str());

        if (!m_is_init) {
            std::string err_info = "RpcChannel not init";
            my_controller->SetError(ERROR_RPC_CHANNEL_INIT, err_info);
            ERRORLOG("%s | %s, RpcChannel not init", req_protocol->m_msg_id.c_str(), err_info.c_str());
            return;
        }

        // request 序列化
        if (!request->SerializeToString(&(req_protocol->m_pb_data))) {
            std::string err_info = "failed to serialize";
            my_controller->SetError(ERROR_FAILED_SERIALIZE, err_info);
            ERRORLOG("%s | %s, origin request [%s]", req_protocol->m_msg_id.c_str(), err_info.c_str(),
                     request->ShortDebugString().c_str());
            return;
        }
        // 编码写入write_buffer
        AbstractCoder* m_coder = m_client->getConnection()->getCoder();
        std::vector<AbstractProtocol::s_ptr> message;
        message.push_back(req_protocol);
        m_coder->encode(message, m_client->getConnection()->getOutBuffer());
        if (!req_protocol->parse_success) {
            my_controller->SetError(ERROR_FAILED_ENCODE, "encode tinypb data error");
            return;
        }
        INFOLOG("============================================================");
        INFOLOG("%s | %s | Set client send request data:%s", req_protocol->m_msg_id.c_str(),
                my_controller->GetPeerAddr()->toString().c_str(), request->ShortDebugString().c_str());
        INFOLOG("============================================================");

        // 调用服务
        TinyPBProtocol::s_ptr res_protocol;
        int rt = m_client->sendAndRecvTinyPb(req_protocol->m_msg_id, res_protocol);
        if (rt != 0) {
            my_controller->SetError(rt, m_client->getConnectErrorInfo());
            ERRORLOG("%s | call rpc occur client error, service_full_name=%s, error_code=%d, error_info=%s", req_protocol->m_msg_id.c_str(),
                     req_protocol->m_method_name.c_str(), rt, m_client->getConnectErrorInfo().c_str());
            return;
        }
        // 反序列化
        if (!response->ParseFromString(res_protocol->m_pb_data)) {
            my_controller->SetError(ERROR_FAILED_DESERIALIZE, "failed to deserialize data from server");
            ERRORLOG("%s | failed to deserialize data", req_protocol->m_msg_id.c_str());
            return;
        }
        if (res_protocol->m_err_code != 0) {
            my_controller->SetError(res_protocol->m_err_code, res_protocol->m_err_info);
            ERRORLOG("%s | server reply error_code=%d, err_info=%s, error_code=%d, error_info=%s", req_protocol->m_msg_id.c_str(),
                     res_protocol->m_err_code, res_protocol->m_err_info.c_str());
            return;
        }

        INFOLOG("============================================================");
        INFOLOG("%s | %s | call rpc server [%s] success.", req_protocol->m_msg_id.c_str(),
                my_controller->GetPeerAddr()->toString().c_str(), req_protocol->m_method_name.c_str());
        INFOLOG("Get server reply response data:%s",response->ShortDebugString().c_str());
        INFOLOG("============================================================");

        if (done) {
            done->Run();
        }
    }

    void RpcChannel::Init(RpcChannel::controller_s_ptr controller, RpcChannel::message_s_ptr req,
                          RpcChannel::message_s_ptr res, RpcChannel::closure_s_ptr done) {

        if (m_is_init) {
            return;
        }
        m_controller = controller;
        m_request = req;
        m_response = res;
        m_closure = done;
        m_is_init = true;
    }

    google::protobuf::RpcController *RpcChannel::getController() {
        return m_controller.get();
    }

    google::protobuf::Message *RpcChannel::getRequest() {
        return m_request.get();
    }

    google::protobuf::Message *RpcChannel::getResponse() {
        return m_response.get();
    }

    google::protobuf::Closure *RpcChannel::getClosure() {
        return m_closure.get();
    }

    TcpClient *RpcChannel::getTcpClient() {
        return m_client.get();
    }

    TimerEvent::s_ptr RpcChannel::getTimerEvent() {
        return m_timer_event;
    }

}