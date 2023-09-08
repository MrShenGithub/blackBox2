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

#ifndef SF_MSGBUS_BLACKBOX2_NODE_PROXY_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_NODE_PROXY_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include <sf-msgbus/blackbox2/node_proxy.h>

#include "handle_proxy_impl.h"
#include "proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class NodeProxyImpl: public ProxyImpl<NodeProxy> {
 public:
    NodeProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer, protocol::Node&& protocol_node);
    ~NodeProxyImpl() override;

 public:
    int GetThreadId() const override;
    const std::string& GetThreadName() const override;
    const std::string& GetName() const override;
    bool IsExecutorAttached() const override;
    std::vector<std::shared_ptr<HandleProxy>> GetHandles() const override;

 public:
    void AddHandleProxy(std::shared_ptr<HandleProxyImpl> handle_proxy_impl);

 protected:
    void HandleActivationChanged(bool is_activated) override;

 private:
    void HandleAttach(google::protobuf::io::ZeroCopyInputStream& input);
    void HandleDetach(google::protobuf::io::ZeroCopyInputStream& input);

 private:
    using HandleInfo = std::pair<std::shared_ptr<HandleProxyImpl>, scoped_connection>;
    using HandleProxyMap = std::map<ENetPeer*, HandleInfo>;

 private:
    protocol::Node protocol_node_;
    bool is_attached_;
    HandleProxyMap handle_proxy_map_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_NODE_PROXY_IMPL_H_
