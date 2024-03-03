#include <google/protobuf/service.h>
#include "rocket/common/log.h"
#include "rocket/common/config.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_server.h"
#include "rocket/net/rpc/rpc_dispatcher.h"

#include "rocket/net/rpc/rpc_registry.h"

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

    std::shared_ptr<rocket::RpcRegistry> service = std::make_shared<rocket::RpcRegistry>();
    rocket::RpcDispatcher::GetRpcDispatcher()->registerService(service);

    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 8888);
    printf("init Tcpserver\n");
//    rocket::TcpServer tcpServer(addr);
//    tcpServer.start();

    rocket::TcpServer::SetTcpServer(addr);
    rocket::TcpServer::GetTcpServer()->start();

    return 0;
}