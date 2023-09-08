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
#include <sf-msgbus/blackbox2/log.h>

#include "enet.h"
#include "channel_stub_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

ChannelStubImpl::ChannelStubImpl(std::shared_ptr<Context> context, const ChannelConfig& channel_config,
                                 Handler inject_message_handler, Stub* process)
    : MessageStubImpl(context, std::move(inject_message_handler), protocol::Opcode::kAttachChannel, protocol_channel_, process) {
    protocol_channel_.set_id(channel_config.properties.at("id"));
    protocol_channel_.set_type(channel_config.properties.at("type"));
    auto it = channel_config.properties.find("dir");
    if (it != channel_config.properties.end()) {
        if (it->second == "in") {
            protocol_channel_.set_dir(protocol::Direction::In);
        } else if (it->second == "out") {
            protocol_channel_.set_dir(protocol::Direction::Out);
        }
    }
    protocol::GetCurrentThread(*protocol_channel_.mutable_owner_thread());
    if (process != nullptr) {
        protocol_channel_.mutable_owner_process()->set_id(process->GetInstanceId());
    }
    auto pmap = protocol_channel_.mutable_config();
    for (auto &kv: channel_config.properties) {
        (*pmap)[kv.first] = kv.second;
    }
    ASBLog(INFO) << this << ": channel stub initialized.";
}

ChannelStubImpl::~ChannelStubImpl() {
    ASBLog(INFO) << this << ": channel stub released.";
}

void ChannelStubImpl::HandleParentInstanceIdChanged(uint64_t id) {
    protocol_channel_.mutable_owner_process()->set_id(id);
}

}
}
}
