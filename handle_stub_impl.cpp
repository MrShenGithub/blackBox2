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

#include "handle_stub_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

HandleStubImpl::HandleStubImpl(std::shared_ptr<Context> context, HandleType handle_type, const std::string& key,
                               const std::map<std::string, std::string>& mapping_channgles,
                               Handler inject_message_handler, Stub* node)
    : MessageStubImpl(context, std::move(inject_message_handler), protocol::Opcode::kAttachHandle, protocol_handle_, node) {
    protocol_handle_.set_key(key);
    protocol::GetCurrentThread(*protocol_handle_.mutable_owner_thread());
    if (node != nullptr) {
        protocol_handle_.mutable_owner_node()->set_id(node->GetInstanceId());
    }
    switch (handle_type) {
    case HandleType::kReader:
        protocol_handle_.set_type(protocol::HandleType::Reader);
        break;
    case HandleType::kWriter:
        protocol_handle_.set_type(protocol::HandleType::Writer);
        break;
    case HandleType::kClient:
        protocol_handle_.set_type(protocol::HandleType::Client);
        break;
    case HandleType::kServer:
        protocol_handle_.set_type(protocol::HandleType::Server);
        break;
    default:
        protocol_handle_.set_type(protocol::HandleType::Unknown);
        break;
    }
    auto* p = protocol_handle_.mutable_mapping_channels();
    for (auto& pair: mapping_channgles) {
        p->insert({ pair.first, pair.second });
    }
}

HandleStubImpl::~HandleStubImpl() {
}

void HandleStubImpl::Enable() {
    protocol::Boolean protocol_boolean;
    protocol_boolean.set_value(true);
    std::lock_guard<std::mutex> lg(GetMutex());
    SendEvent(protocol::Opcode::kHandleEnable, protocol_boolean);
}

void HandleStubImpl::Disable() {
    protocol::Boolean protocol_boolean;
    protocol_boolean.set_value(false);
    std::lock_guard<std::mutex> lg(GetMutex());
    SendEvent(protocol::Opcode::kHandleEnable, protocol_boolean);
}

void HandleStubImpl::HandleParentInstanceIdChanged(uint64_t id) {
    protocol_handle_.mutable_owner_node()->set_id(id);
}

}  // namespace blackbox
}  // namespace msgbus
}  // namespace asf
