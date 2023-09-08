// -----------------------------------------------------------------------
// |             _     _              _____         _____                |
// |            |  \  | |            / ____|  /\   |  __ \               |
// |            | | \ | |  __       | (___   /  \  | |__) |              |
// |            | |\ \| | /__\|   |  \___ \ / /\ \ |  _  /               |
// |            | | \ \ ||    |   |   ___) / /__\ \| | \ \               |
// |            |_|  \_\| \__/ \_/|/|_____/________\_|  \_\              |
// |                                                                     |
// -----------------------------------------------------------------------
// COPYRIGHT
// -----------------------------------------------------------------------
//
// This software is copyright protected and proprietary to Neusoft Reach.
// Neusoft Reach grants to you only those rights as set out in the license
// conditions.
// All other rights remain with Neusoft Reach.
// -----------------------------------------------------------------------

#ifndef SF_MSGBUS_BLACKBOX2_EXECUTOR_PROXY_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_EXECUTOR_PROXY_IMPL_H_

#include <list>

#include <sf-msgbus/blackbox2/executor_proxy.h>

#include "proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class ExecutorProxyImpl: public ProxyImpl<ExecutorProxy> {
 public:
    ExecutorProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer, protocol::Executor&& protocol_executor);
    ~ExecutorProxyImpl() override;

 public:
    int GetThreadId() const override;
    const std::string& GetThreadName() const override;
    size_t GetThreadPoolSize() const override;
    bool IsRunning() const override;
    std::vector<std::string> GetAttachedNodes() const override;

 private:
    void HandleAttachNode(google::protobuf::io::ZeroCopyInputStream& input);
    void HandleDetachNode(google::protobuf::io::ZeroCopyInputStream& input);
    void HandleRunBegin(google::protobuf::io::ZeroCopyInputStream& input);
    void HandleRunEnd(google::protobuf::io::ZeroCopyInputStream& input);
    void HandleTaskBegin(google::protobuf::io::ZeroCopyInputStream& input);
    void HandleTaskEnd(google::protobuf::io::ZeroCopyInputStream& input);

 private:
    protocol::Executor protocol_executor_;
    bool is_running_;
    std::list<std::string> attached_nodes_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_EXECUTOR_PROXY_IMPL_H_
