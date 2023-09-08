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

#include "executor_proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

ExecutorProxyImpl::ExecutorProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer,
                                     protocol::Executor&& protocol_executor)
    : ProxyImpl(context, enet_peer)
    , protocol_executor_(std::move(protocol_executor)) {
    is_running_ = protocol_executor_.is_runnning();
    auto& attached_nodes = protocol_executor_.attached_nodes();
    for (auto attached_node: attached_nodes) {
        attached_nodes_.push_back(attached_node);
    }
    ProxyImpl::RegisterEventHandler(protocol::Opcode::kExecutorAttachNode, std::bind(&ExecutorProxyImpl::HandleAttachNode, this, std::placeholders::_1));
    ProxyImpl::RegisterEventHandler(protocol::Opcode::kExecutorDetachNode, std::bind(&ExecutorProxyImpl::HandleDetachNode, this, std::placeholders::_1));
    ProxyImpl::RegisterEventHandler(protocol::Opcode::kExecutorRunBegin, std::bind(&ExecutorProxyImpl::HandleRunBegin, this, std::placeholders::_1));
    ProxyImpl::RegisterEventHandler(protocol::Opcode::kExecutorRunEnd, std::bind(&ExecutorProxyImpl::HandleRunEnd, this, std::placeholders::_1));
    ProxyImpl::RegisterEventHandler(protocol::Opcode::kExecutorTaskBegin, std::bind(&ExecutorProxyImpl::HandleTaskBegin, this, std::placeholders::_1));
    ProxyImpl::RegisterEventHandler(protocol::Opcode::kExecutorTaskEnd, std::bind(&ExecutorProxyImpl::HandleTaskEnd, this, std::placeholders::_1));
}

ExecutorProxyImpl::~ExecutorProxyImpl() {
}

int ExecutorProxyImpl::GetThreadId() const {
    return protocol_executor_.owner_thread().id();
}

const std::string& ExecutorProxyImpl::GetThreadName() const {
    return protocol_executor_.owner_thread().name();
}

size_t ExecutorProxyImpl::GetThreadPoolSize() const {
    return protocol_executor_.thread_pool_size();
}

bool ExecutorProxyImpl::IsRunning() const {
    return is_running_;
}

std::vector<std::string> ExecutorProxyImpl::GetAttachedNodes() const {
    std::lock_guard<std::mutex> lg(GetMutex());
    std::vector<std::string> attached_nodes;
    attached_nodes.reserve(attached_nodes_.size());
    for (auto& attached_node: attached_nodes_) {
        attached_nodes.push_back(attached_node);
    }
    return attached_nodes;
}

void ExecutorProxyImpl::HandleAttachNode(google::protobuf::io::ZeroCopyInputStream& input) {
    protocol::String protocol_string;
    if (protocol_string.ParseFromZeroCopyStream(&input)) {
        {
            std::lock_guard<std::mutex> lg(GetMutex());
            attached_nodes_.emplace_back(protocol_string.value());
        }
        OnNodeAttached(protocol_string.value());
    } else {
        ASBLog(ERROR) << "Failed to parse executor attach node event.";
    }
}

void ExecutorProxyImpl::HandleDetachNode(google::protobuf::io::ZeroCopyInputStream& input) {
    protocol::String protocol_string;
    if (protocol_string.ParseFromZeroCopyStream(&input)) {
        {
            std::lock_guard<std::mutex> lg(GetMutex());
            std::remove(attached_nodes_.begin(), attached_nodes_.end(), protocol_string.value());
        }
        OnNodeDetached(protocol_string.value());
    } else {
        ASBLog(ERROR) << "Failed to parse executor detach node event.";
    }
}

void ExecutorProxyImpl::HandleRunBegin(google::protobuf::io::ZeroCopyInputStream& input) {
    {
        std::lock_guard<std::mutex> lg(GetMutex());
        is_running_ = true;
    }

    ASBLog(INFO) << "is_running " << is_running_;

    protocol::Thread protocol_thread;
    if (protocol_thread.ParseFromZeroCopyStream(&input)) {
        OnRunBegin(protocol_thread.id(), protocol_thread.name());
    } else {
        ASBLog(ERROR) << "Failed to parse executor run begin event.";
    }
}

void ExecutorProxyImpl::HandleRunEnd(google::protobuf::io::ZeroCopyInputStream& input) {
    std::lock_guard<std::mutex> lg(GetMutex());
    if (is_running_) {
        is_running_ = false;
        ASBLog(INFO) << "is_running " << is_running_;
        ScopedUnlocker<std::mutex> unlocker(GetMutex());
        OnRunEnd();
    }
}

void ExecutorProxyImpl::HandleTaskBegin(google::protobuf::io::ZeroCopyInputStream& input) {
    protocol::ExecutorTask task;
    if (task.ParseFromZeroCopyStream(&input)) {
        OnTaskBegin(task.task_id(), task.thread().id(), task.thread().name());
    } else {
        ASBLog(ERROR) << "Failed to parse executo task begin event.";
    }
}

void ExecutorProxyImpl::HandleTaskEnd(google::protobuf::io::ZeroCopyInputStream& input) {
    protocol::ExecutorTask task;
    if (task.ParseFromZeroCopyStream(&input)) {
        OnTaskEnd(task.task_id());
    } else {
        ASBLog(ERROR) << "Failed to parse executo task end event.";
    }
}

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf
