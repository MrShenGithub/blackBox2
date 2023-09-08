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

#include <sf-msgbus/types/scope_guard.h>
#include <sf-msgbus/types/scoped_unlocker.h>
#include <sf-msgbus/blackbox2/log.h>

#include "enet.h"
#include "channel_proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

ChannelProxyImpl::ChannelProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer, protocol::Channel&& protocol_channel)
    : MessageProxyImpl(context, enet_peer)
    , protocol_channel_(std::move(protocol_channel)) {
    auto& configs = protocol_channel_.config();
    for (auto& config: configs) {
        properties_[config.first] = config.second;
    }
}

ChannelProxyImpl::~ChannelProxyImpl() {
}

int ChannelProxyImpl::GetThreadId() const {
    return protocol_channel_.owner_thread().id();
}

const std::string& ChannelProxyImpl::GetThreadName() const {
    return protocol_channel_.owner_thread().name();
}

const std::string& ChannelProxyImpl::GetId() const {
    return protocol_channel_.id();
}

const std::string& ChannelProxyImpl::GetType() const {
    return protocol_channel_.type();
}

const ChannelProxy::Properties& ChannelProxyImpl::GetProperties() const {
    return properties_;
}

bool ChannelProxyImpl::IsCompatibleWith(const Properties& properties) const {
    assert(false); // Not impl
    return false;
}

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf
