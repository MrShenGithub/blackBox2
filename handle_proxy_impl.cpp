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

#include "handle_proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

HandleProxyImpl::HandleProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer, protocol::Handle&& protocol_handle)
    : MessageProxyImpl<HandleProxy>(context, enet_peer)
    , protocol_handle_(std::move(protocol_handle)) {
    is_enabled_ = protocol_handle_.is_enabled();
    auto& protocol_mapping_channels = protocol_handle_.mapping_channels();
    for (auto& protocol_mapping_channel: protocol_mapping_channels) {
        mapping_channels_[protocol_mapping_channel.first] = protocol_mapping_channel.second;
    }
    switch (protocol_handle_.type()) {
    case protocol::HandleType::Reader:
        type_ = HandleType::kReader;
        break;
    case protocol::HandleType::Writer:
        type_ = HandleType::kWriter;
        break;
    case protocol::HandleType::Client:
        type_ = HandleType::kClient;
        break;
    case protocol::HandleType::Server:
        type_ = HandleType::kServer;
        break;
    default:
        type_ = HandleType::kInvalid;
        break;
    }
    MessageProxyImpl::RegisterEventHandler(protocol::Opcode::kHandleEnable, std::bind(&HandleProxyImpl::HandleEnable, this, std::placeholders::_1));
    MessageProxyImpl::RegisterEventHandler(protocol::Opcode::kHandleDisable, std::bind(&HandleProxyImpl::HandleDisable, this, std::placeholders::_1));
}

HandleProxyImpl::~HandleProxyImpl() {
}

int HandleProxyImpl::GetThreadId() const {
    return protocol_handle_.owner_thread().id();
}

const std::string& HandleProxyImpl::GetThreadName() const {
    return protocol_handle_.owner_thread().name();
}

HandleType HandleProxyImpl::GetType() const {
    return type_;
}

const std::string& HandleProxyImpl::GetKey() const {
    return protocol_handle_.key();
}

bool HandleProxyImpl::IsEnabled() const {
    return is_enabled_;
}

const std::map<std::string, std::string>& HandleProxyImpl::GetMappingChannels() const {
    return mapping_channels_;
}

void HandleProxyImpl::HandleEnable(google::protobuf::io::ZeroCopyInputStream& input) {
    is_enabled_ = true;
    ScopedUnlocker<std::mutex> unlocker(GetMutex());
    OnEnabled();
}

void HandleProxyImpl::HandleDisable(google::protobuf::io::ZeroCopyInputStream& input) {
    is_enabled_ = false;
    ScopedUnlocker<std::mutex> unlocker(GetMutex());
    OnDisabled();
}

}  // namespace blackbox
}  // namespace msgbus
}  // namespace asf
