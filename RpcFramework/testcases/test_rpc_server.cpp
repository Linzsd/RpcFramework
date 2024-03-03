#include <google/protobuf/service.h>
#include "rocket/common/log.h"
#include "rocket/common/config.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_server.h"
#include "rocket/net/rpc/rpc_dispatcher.h"
#include "rocket/net/rpc/rpc_channel.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/rpc/rpc_closure.h"

#include "order.pb.h"
#include "rocket/net/rpc/register_server.pb.h"

class OrderImpl : public Order {
    void makeOrder(google::protobuf::RpcController* controller,
                   const ::makeOrderRequest* request,
                   ::makeOrderResponse* response,
                   ::google::protobuf::Closure* done) {

//        APPDEBUGLOG("start sleep 2s");
//        sleep(2);
//        APPDEBUGLOG("end sleep 2s");
        if (request->price() < 10) {
            response->set_ret_code(-1);
            response->set_res_info("short balance");
            return;
        }
        response->set_order_id("20231225");
        APPDEBUGLOG("call makeOrder success");
    }
};

void registerToRegistry() {
    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 8888); // 注册中心port:8888
    rocket::RpcChannel channel(addr);
    DEBUGLOG("input an integer to set count that send tinypb data");
//    DebugLog << "==========================test no:" << n;
    std::shared_ptr<RegisterServerReq> request = std::make_shared<RegisterServerReq>();
    std::shared_ptr<RegisterServerResp> response = std::make_shared<RegisterServerResp>();
    request->set_servicename("Order");
    request->set_serviceipandport("127.0.0.1:12345");

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

    stub.registerServer(rpc_controller.get(), request.get(), response.get(), closure.get());

    if (rpc_controller->GetErrorCode() != 0) {
        ERRORLOG("call rpc method query_name failed, errcode=%d, error=%s", rpc_controller->GetErrorCode(),
                 rpc_controller->ErrorText().c_str());
    }
    if (response->ret_code() != 0) {
        ERRORLOG("query name error, errcode=%d, res_info=%s", response->ret_code(), response->res_info().c_str());
    } else {
        DEBUGLOG("success register to registry")
    }
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        printf("Start test_rpc_server error, argc not 2\n");
        printf("Start like this: \n");
        printf("./test_rpc_server ../conf/rocket.xml \n");
        return 0;
    }

    rocket::Config::SetGlobalConfig(argv[1]);

    printf("init Logger\n");
    rocket::Logger::InitGlobalLogger(0);

    std::shared_ptr<OrderImpl> service = std::make_shared<OrderImpl>();
    rocket::RpcDispatcher::GetRpcDispatcher()->registerService(service);

    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", rocket::Config::GetGlobalConfig()->m_port);
    printf("init Tcpserver\n");
//    rocket::TcpServer tcpServer(addr);
//    tcpServer.start();

    rocket::Coroutine::s_ptr cor = rocket::CoroutinePool::GetCoroutinePool()->getCoroutineInstanse();
    cor->setCallBack(&registerToRegistry);

    rocket::TcpServer::SetTcpServer(addr);
    rocket::TcpServer::GetTcpServer()->addCoroutine(cor);
    rocket::TcpServer::GetTcpServer()->start();

    return 0;
}