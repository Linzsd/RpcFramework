
#include "rocket/net/rpc/rpc_registry.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/common/log.h"



namespace rocket {


    void RpcRegistry::getTargetAddr(::google::protobuf::RpcController *controller, const ::GetTargetAddrReq *request,
                                    ::GetTargetAddrResp *response, ::google::protobuf::Closure *done) {

        ScopeMutex<Mutex> lock(m_mutex);
        auto addrs = allService[request->targetservicename()];
        lock.unlock();

        for (auto addr : addrs) {
            response->add_targetserviceipandport(addr);
        }

    }

    void RpcRegistry::registerServer(::google::protobuf::RpcController *controller, const ::RegisterServerReq *request,
                                     ::RegisterServerResp *response, ::google::protobuf::Closure *done) {

        // 注册服务
        ScopeMutex<Mutex> lock(m_mutex);
        allService[request->servicename()].push_back(request->serviceipandport());
        lock.unlock();
    }


}