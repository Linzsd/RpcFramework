#include <string>
#include "rocket/common/log.h"
#include "rocket/common/config.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/tcp/tcp_server.h"
#include "rocket/net/rpc/rpc_channel.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/rpc/rpc_closure.h"
#include "rocket/coroutine/coroutine_pool.h"

#include "order.pb.h"
#include "rocket/net/rpc/register_server.pb.h"

//void test_tcp_client() {
//    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 12345);
//    rocket::TcpClient client(addr);
//    client.toConnect([addr, &client]() {
//        DEBUGLOG("connect to [%s] success", addr->toString().c_str());
//        std::shared_ptr<rocket::TinyPBProtocol> message = std::make_shared<rocket::TinyPBProtocol>();
//        message->m_msg_id = "99998888";
//        message->m_pb_data = "test pb data";
//
//        makeOrderRequest request;
//        request.set_price(100);
//        request.set_goods("apple");
//
//        if (!request.SerializeToString(&(message->m_pb_data))) {
//            ERRORLOG("serialize error");
//            return;
//        }
//
//        message->m_method_name = "Order.makeOrder";
//
//        client.writeMessage(message, [request](rocket::AbstractProtocol::s_ptr msg_ptr) {
//            DEBUGLOG("send message success, request[%s]", request.ShortDebugString().c_str());
//        });
//
//        client.readMessage("99998888", [](rocket::AbstractProtocol::s_ptr msg_ptr) {
//            std::shared_ptr<rocket::TinyPBProtocol> message = std::dynamic_pointer_cast<rocket::TinyPBProtocol>(msg_ptr);
//            DEBUGLOG("msg_id[%s], get response %s", message->m_msg_id.c_str(), message->m_pb_data.c_str());
//            makeOrderRequest response;
//            if (!response.ParseFromString(message->m_pb_data)) {
//                ERRORLOG("deserialize error");
//                return;
//            }
//            DEBUGLOG("get response success, response[%s]", response.ShortDebugString().c_str());
//        });
//
//    });
//}

//void test_channel() {
////    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 12345);
////    std::shared_ptr<rocket::RpcChannel> channel = std::make_shared<rocket::RpcChannel>(addr);
//
////    std::shared_ptr<makeOrderRequest> request = std::make_shared<makeOrderRequest>();
////    std::shared_ptr<makeOrderResponse> response = std::make_shared<makeOrderResponse>();
//
//    NEWRPCCHANNEL("127.0.0.1:12345", channel);
//    NEWMESSAGE(makeOrderRequest, request);
//    NEWMESSAGE(makeOrderResponse, response);
//
//    request->set_price(100);
//    request->set_goods("apple");
//
//
////    std::shared_ptr<rocket::RpcController> controller = std::make_shared<rocket::RpcController>();
//    NEWRPCCONTROLLER(controller);
//    controller->SetMsgId("99998888");
//    controller->SetTimeout(10000);
//
//    std::shared_ptr<rocket::RpcClosure> closure = std::make_shared<rocket::RpcClosure>([request, response, channel, controller]() mutable {
//        DEBUGLOG("error code = %d", controller->GetErrorCode())
//        if (controller->GetErrorCode() == 0) {
//            INFOLOG("call rpc success, request[%s], response[%s]", request->ShortDebugString().c_str(),
//                    response->ShortDebugString().c_str());
//            // 执行业务逻辑
//            if (response->order_id() == "xxx") {
//                // xx
//            }
//        } else {
//            ERRORLOG("call rpc failed, request[%s], error code[%d], error info[%s]", request->ShortDebugString().c_str(),
//                    controller->GetErrorCode(), controller->GetErrorInfo().c_str());
//        }
//
//        INFOLOG("now exit eventloop");
//        channel->getTcpClient()->stop();
//        channel.reset();
//    });
//
////    channel->Init(controller, request, response, closure);
////
////    Order_Stub stub(channel.get());
////
////    stub.makeOrder(controller.get(), request.get(), response.get(), closure.get());
//
//    CALLRPC("127.0.0.1:12345", makeOrder, controller, request, response, closure);
//}

void fun() {
    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 12345);
    rocket::RpcChannel channel(addr);
    DEBUGLOG("input an integer to set count that send tinypb data");
    std::shared_ptr<makeOrderRequest> request = std::make_shared<makeOrderRequest>();
    std::shared_ptr<makeOrderResponse> response = std::make_shared<makeOrderResponse>();
    request->set_price(100);
    request->set_goods("apple");

    std::shared_ptr<rocket::RpcClosure> closure = std::make_shared<rocket::RpcClosure>([]() {
        DEBUGLOG("==========================");
        DEBUGLOG("succ call rpc");
        DEBUGLOG("==========================");
    });

    Order_Stub stub(&channel);
    std::shared_ptr<rocket::RpcController> rpc_controller = std::make_shared<rocket::RpcController>();
    rpc_controller->SetMsgId("99998888");
    rpc_controller->SetTimeout(5000);

    channel.Init(rpc_controller, request, response, closure);

    stub.makeOrder(rpc_controller.get(), request.get(), response.get(), closure.get());

    if (rpc_controller->GetErrorCode() != 0) {
        ERRORLOG("call rpc method query_name failed, errcode=%d, error=%s", rpc_controller->GetErrorCode(),
                 rpc_controller->ErrorText().c_str());
    }
    if (response->ret_code() != 0) {
        ERRORLOG("query name error, errcode=%d, res_info=%s", response->ret_code(), response->res_info().c_str());
    } else {
        DEBUGLOG("get response.order_id = %s", response->order_id().c_str())
    }
}
//
void fun1() {
    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 8888); // 连接注册中心
    rocket::RpcChannel channel(addr);
    DEBUGLOG("input an integer to set count that send tinypb data");
    std::shared_ptr<GetTargetAddrReq> request = std::make_shared<GetTargetAddrReq>();
    std::shared_ptr<GetTargetAddrResp> response = std::make_shared<GetTargetAddrResp>();
    request->set_sourceservicename("client");
    request->set_targetservicename("Order");

    std::shared_ptr<rocket::RpcClosure> closure = std::make_shared<rocket::RpcClosure>([]() {
        DEBUGLOG("==========================");
        DEBUGLOG("succ call rpc");
        DEBUGLOG("==========================");
    });

    Register_Stub stub(&channel);
    std::shared_ptr<rocket::RpcController> rpc_controller = std::make_shared<rocket::RpcController>();
    rpc_controller->SetMsgId("99998888");
    rpc_controller->SetTimeout(5000);

    channel.Init(rpc_controller, request, response, closure);

    stub.getTargetAddr(rpc_controller.get(), request.get(), response.get(), closure.get());

    if (rpc_controller->GetErrorCode() != 0) {
        ERRORLOG("call rpc method query_name failed, errcode=%d, error=%s", rpc_controller->GetErrorCode(),
                 rpc_controller->ErrorText().c_str());
    }
    if (response->ret_code() != 0) {
        ERRORLOG("query name error, errcode=%d, res_info=%s", response->ret_code(), response->res_info().c_str());
    } else {
        std::vector<std::string> cache;
        for (int i = 0; i < response->targetserviceipandport_size(); ++i) {
            cache.push_back(response->targetserviceipandport(i));
            DEBUGLOG("service addr = %s", response->targetserviceipandport(i).c_str());
        }
        rocket::TcpServer::GetTcpServer()->updateService(request->targetservicename(), cache);
    }
}

void fun2() {
    fun1();
    std::string IP_Port = rocket::TcpServer::GetTcpServer()->getServiceWithLoadBalance("Order");
    DEBUGLOG("get target IP and Port : %s", IP_Port.c_str());
    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>(IP_Port);
    rocket::RpcChannel channel(addr);
    DEBUGLOG("input an integer to set count that send tinypb data");
    std::shared_ptr<makeOrderRequest> request = std::make_shared<makeOrderRequest>();
    std::shared_ptr<makeOrderResponse> response = std::make_shared<makeOrderResponse>();
    request->set_price(100);
    request->set_goods("apple");

    std::shared_ptr<rocket::RpcClosure> closure = std::make_shared<rocket::RpcClosure>([]() {
        DEBUGLOG("==========================");
        DEBUGLOG("succ call rpc");
        DEBUGLOG("==========================");
    });

    Order_Stub stub(&channel);
    std::shared_ptr<rocket::RpcController> rpc_controller = std::make_shared<rocket::RpcController>();
    rpc_controller->SetMsgId("99998888");
    rpc_controller->SetTimeout(5000);

    channel.Init(rpc_controller, request, response, closure);

    stub.makeOrder(rpc_controller.get(), request.get(), response.get(), closure.get());

    if (rpc_controller->GetErrorCode() != 0) {
        ERRORLOG("call rpc method query_name failed, errcode=%d, error=%s", rpc_controller->GetErrorCode(),
                 rpc_controller->ErrorText().c_str());
    }
    if (response->ret_code() != 0) {
        ERRORLOG("query name error, errcode=%d, res_info=%s", response->ret_code(), response->res_info().c_str());
    } else {
        DEBUGLOG("get response.order_id = %s", response->order_id().c_str())
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Start RPC server error, input argc is not 2!");
        printf("Start RPC server like this: \n");
        printf("./server a.xml\n");
        return 0;
    }

    rocket::Config::SetGlobalConfig(argv[1]);
    printf("init Logger\n");
    rocket::Logger::InitGlobalLogger(0);

    rocket::Coroutine::s_ptr cor = rocket::CoroutinePool::GetCoroutinePool()->getCoroutineInstanse();
    cor->setCallBack(&fun2);

    rocket::IPNetAddr::s_ptr addr_server = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 12346);
    rocket::TcpServer::SetTcpServer(addr_server);

    rocket::TcpServer::GetTcpServer()->addCoroutine(cor);

    rocket::TcpServer::GetTcpServer()->start();
//    fun();

    return 0;
}