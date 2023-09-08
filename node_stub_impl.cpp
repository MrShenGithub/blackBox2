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
#include "node_stub_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

NodeStubImpl::NodeStubImpl(std::shared_ptr<Context> context, const std::string& name, Stub* process)
    : StubImpl(context, protocol::Opcode::kAttachNode, protocol_node_, process) {
    protocol_node_.set_name(name);
    protocol_node_.set_is_attached(false);
    protocol::GetCurrentThread(*protocol_node_.mutable_owner_thread());
    if (process != nullptr) {
        protocol_node_.mutable_owner_process()->set_id(process->GetInstanceId());
    }
}

NodeStubImpl::~NodeStubImpl() {
}

const std::string& NodeStubImpl::GetName() const {
    return protocol_node_.name();
}

void NodeStubImpl::Attach() {
    std::lock_guard<std::mutex> lg(GetMutex());
    protocol_node_.set_is_attached(true);
    SendEvent(protocol::Opcode::kNodeAttach);
}

void NodeStubImpl::Detach() {
    std::lock_guard<std::mutex> lg(GetMutex());
    protocol_node_.set_is_attached(false);
    SendEvent(protocol::Opcode::kNodeDetach);
}

std::shared_ptr<HandleStub> NodeStubImpl::CreateHandleStub(HandleType handle_type, const std::string& key,
                                                           const std::map<std::string, std::string>& mapping_channels,
                                                           HandleStub::Handler inject_message_handler) {
    if (GetContext()->IsEnabled()) {
        auto handle_stub_impl = std::make_shared<HandleStubImpl>(GetContext(), handle_type, key, mapping_channels,
                                    std::move(inject_message_handler), this);
        if (handle_stub_impl && handle_stub_impl->Start()) {
            return handle_stub_impl;
        }
    }
    return nullptr;
}

void NodeStubImpl::HandleParentInstanceIdChanged(uint64_t id) {
    protocol_node_.mutable_owner_process()->set_id(id);
}

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf
