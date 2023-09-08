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

#include <sf-msgbus/blackbox2/log.h>

#include "node_proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

NodeProxyImpl::NodeProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer, protocol::Node&& protocol_node)
    : ProxyImpl(context, enet_peer)
    , protocol_node_(std::move(protocol_node)) {
    is_attached_ = protocol_node.is_attached();
    ProxyImpl::RegisterEventHandler(protocol::Opcode::kNodeAttach, std::bind(&NodeProxyImpl::HandleAttach, this, std::placeholders::_1));
    ProxyImpl::RegisterEventHandler(protocol::Opcode::kNodeDetach, std::bind(&NodeProxyImpl::HandleDetach, this, std::placeholders::_1));
}

NodeProxyImpl::~NodeProxyImpl() {
}

int NodeProxyImpl::GetThreadId() const {
    return protocol_node_.owner_thread().id();
}

const std::string& NodeProxyImpl::GetThreadName() const {
    return protocol_node_.owner_thread().name();
}

const std::string& NodeProxyImpl::GetName() const {
    return protocol_node_.name();
}

bool NodeProxyImpl::IsExecutorAttached() const {
    return is_attached_;
}

std::vector<std::shared_ptr<HandleProxy>> NodeProxyImpl::GetHandles() const {
    std::lock_guard<std::mutex> lg(GetMutex());
    std::vector<std::shared_ptr<HandleProxy>> handles;
    handles.reserve(handle_proxy_map_.size());
    for (auto& it: handle_proxy_map_) {
        handles.push_back(it.second.first);
    }
    return handles;
}

void NodeProxyImpl::AddHandleProxy(std::shared_ptr<HandleProxyImpl> handle_proxy_impl) {
    std::lock_guard<std::mutex> lg(GetMutex());
    auto enet_peer = handle_proxy_impl->GetENetPeer();
    auto it = handle_proxy_map_.find(enet_peer);
    if (it != handle_proxy_map_.end()) {
        ASBLog(ERROR) << "Duplicated handle";
        return;
    }
    auto connection = handle_proxy_impl->OnDisconnected.connect([this, enet_peer] {
        std::lock_guard<std::mutex> lg(GetMutex());
        auto it = handle_proxy_map_.find(enet_peer);
        if (it != handle_proxy_map_.end()) {
            auto handle_proxy_impl = it->second.first;
            handle_proxy_map_.erase(it);
            ScopedUnlocker<std::mutex> unlocker(GetMutex());
            OnHandleRemoved(handle_proxy_impl);
        }
    });
    handle_proxy_map_[enet_peer] = std::make_pair(handle_proxy_impl, connection);
    ScopedUnlocker<std::mutex> unlocker(GetMutex());
    OnHandleAdded(handle_proxy_impl);
}

void NodeProxyImpl::HandleActivationChanged(bool is_activated) {
    for (auto it = handle_proxy_map_.begin(); it != handle_proxy_map_.end(); ++it) {
        it->second.first->SetActivation(is_activated);
    }
}

void NodeProxyImpl::HandleAttach(google::protobuf::io::ZeroCopyInputStream& input) {
    {
        std::lock_guard<std::mutex> lg(GetMutex());
        is_attached_ = true;
    }
    OnExecutorAttached();
}

void NodeProxyImpl::HandleDetach(google::protobuf::io::ZeroCopyInputStream& input) {
    {
        std::lock_guard<std::mutex> lg(GetMutex());
        is_attached_ = false;
    }
    OnExecutorDetached();
}

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf
