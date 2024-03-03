#ifndef ROCKET_RPC_REGISTRY_H
#define ROCKET_RPC_REGISTRY_H

#include <unordered_map>
#include <vector>
#include <google/protobuf/service.h>
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/rpc/register_server.pb.h"
#include "rocket/common/mutex.h"

namespace rocket {


    class RpcRegistry : public Register {
    public:
        RpcRegistry() = default;

        ~RpcRegistry() = default;

        void getTargetAddr(::google::protobuf::RpcController* controller,
                           const ::GetTargetAddrReq* request,
                           ::GetTargetAddrResp* response,
                           ::google::protobuf::Closure* done);

        void registerServer(::google::protobuf::RpcController* controller,
                            const ::RegisterServerReq* request,
                            ::RegisterServerResp* response,
                            ::google::protobuf::Closure* done);

    private:
        Mutex m_mutex;
        std::unordered_map<std::string, std::vector<std::string>> allService; // value中的vector存储ip:port, 127.0.0.1:8080
        std::unordered_map<std::string, int> serviceCnt; // key = 源服务名 + 目标服务名, 判断要不要做限流熔断
        std::unordered_map<std::string, std::unordered_map<int, int>> curConnects;  // 记录每一个实例当前承载的连接数，第二个map,key=实例号，value=数量

    };

}


#endif //ROCKET_RPC_REGISTRY_H
