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

#include "executor_stub_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

ExecutorStubImpl::ExecutorStubImpl(std::shared_ptr<Context> context, size_t thread_pool_size, Stub* process)
    : StubImpl(context, protocol::Opcode::kAttachExecutor, protocol_executor_, process) {
    protocol_executor_.set_thread_pool_size(thread_pool_size);
    protocol_executor_.set_is_runnning(false);
    protocol::GetCurrentThread(*protocol_executor_.mutable_owner_thread());
    if (process != nullptr) {
        protocol_executor_.mutable_owner_process()->set_id(process->GetInstanceId());
    }
}

ExecutorStubImpl::~ExecutorStubImpl() {
}

void ExecutorStubImpl::AttachNode(std::shared_ptr<NodeStub> node_stub) {
    std::lock_guard<std::mutex> lg(GetMutex());
    protocol::String node_name;
    node_name.set_value(node_stub->GetName());
    SendEvent(protocol::Opcode::kExecutorAttachNode, node_name);
    protocol_executor_.add_attached_nodes(node_stub->GetName());
}

void ExecutorStubImpl::DetachNode(std::shared_ptr<NodeStub> node_stub) {
    std::lock_guard<std::mutex> lg(GetMutex());
    protocol::String node_name;
    node_name.set_value(node_stub->GetName());
    SendEvent(protocol::Opcode::kExecutorDetachNode, node_name);
    auto *p = protocol_executor_.mutable_attached_nodes();
    for (auto it = p->begin(); it != p->end(); ++it) {
        if ((*it) == node_stub->GetName()) {
            p->erase(it);
            break;
        }
    }
}

void ExecutorStubImpl::RunBegin() {
    std::lock_guard<std::mutex> lg(GetMutex());
    protocol::Thread protocol_thread;
    protocol::GetCurrentThread(protocol_thread);
    SendEvent(protocol::Opcode::kExecutorRunBegin, protocol_thread);
    protocol_executor_.set_is_runnning(true);
}

void ExecutorStubImpl::RunEnd() {
    std::lock_guard<std::mutex> lg(GetMutex());
    protocol::Thread protocol_thread;
    protocol::GetCurrentThread(protocol_thread);
    SendEvent(protocol::Opcode::kExecutorRunEnd, protocol_thread);
    protocol_executor_.set_is_runnning(false);
}

void ExecutorStubImpl::TaskBegin(int task_id) {
    std::lock_guard<std::mutex> lg(GetMutex());
    protocol::ExecutorTask protocol_executor_task;
    protocol::GetCurrentThread(*protocol_executor_task.mutable_thread());
    protocol_executor_task.set_task_id(task_id);
    SendEvent(protocol::Opcode::kExecutorTaskBegin, protocol_executor_task);
}

void ExecutorStubImpl::TaskEnd(int task_id) {
    std::lock_guard<std::mutex> lg(GetMutex());
    protocol::ExecutorTask protocol_executor_task;
    protocol::GetCurrentThread(*protocol_executor_task.mutable_thread());
    protocol_executor_task.set_task_id(task_id);
    SendEvent(protocol::Opcode::kExecutorTaskEnd, protocol_executor_task);
}

void ExecutorStubImpl::HandleParentInstanceIdChanged(uint64_t id) {
    protocol_executor_.mutable_owner_process()->set_id(id);
}

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf
