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

#include <map>
#include <set>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include <sf-msgbus/types/scope_guard.h>
#include <sf-msgbus/types/scoped_unlocker.h>
#include <sf-msgbus/blackbox2/server.h>
#include <sf-msgbus/blackbox2/log.h>

#include "context.h"
#include "process_proxy_impl.h"
#include "channel_proxy_impl.h"
#include "executor_proxy_impl.h"
#include "node_proxy_impl.h"
#include "handle_proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

using namespace std::placeholders;
using namespace std::chrono_literals;

class Server::Impl final {
 public:
    Impl(const std::string& host, uint16_t port,
         signal<std::shared_ptr<ProcessProxy>>& pas, signal<std::shared_ptr<ProcessProxy>>& prs)
        : on_process_added_(pas)
        , on_process_removed_(prs)
        , host_(host)
        , port_(port) {
        ASBLog(INFO) << "Server " << this << ": " << host_ << ":" << port_;
    }

    ~Impl() {
        ASBLog(INFO) << "Server " << this << ": released.";
    }

 public:
    bool Start() {
        ASBLog(INFO) << "Server " << this << ": starting blackbox server...";

        std::lock_guard<std::mutex> lg(mutex_);

        ENetAddress enet_address;
        enet_address.port = port_;
        if (host_ == "any") {
            enet_address.host = ENET_HOST_ANY;
        } else {
            enet_address_set_host(&enet_address, host_.c_str());
        }

        auto context = std::make_shared<Context>();
        if (!context->StartAsServer(std::bind(&Impl::HandleConnect, this, _1))) {
            return false;
        }

        context_ = context;

        return true;
    }

    void Stop() {
        std::shared_ptr<Context> context;
        {
            std::lock_guard<std::mutex> lg(mutex_);
            context = context_;
            context_.reset();
        }
        ASBLog(INFO) << "Server " << this << ": Stopping blackbox server...";
        context->Stop();
    }

    std::vector<std::shared_ptr<ProcessProxy>> GetProcesses() const {
        std::lock_guard<std::mutex> lg(mutex_);
        std::vector<std::shared_ptr<ProcessProxy>> processes;
        processes.reserve(process_proxy_map_.size());
        for (auto& it: process_proxy_map_) {
            processes.push_back(it.second.first);
        }
        return processes;
    }

 private:
    void HandleConnect(ENetPeer* enet_peer) {
        ASBLog(INFO) << "New peer " << enet_peer;
        std::lock_guard<std::mutex> lg(mutex_);
        context_->RegisterRequestHandler(enet_peer, protocol::Opcode::kAttachProcess,
                                         std::bind(&Impl::HandleAttachProcess, this, std::placeholders::_1));
        context_->RegisterRequestHandler(enet_peer, protocol::Opcode::kAttachChannel,
                                         std::bind(&Impl::HandleAttachChannel, this, std::placeholders::_1));
        context_->RegisterRequestHandler(enet_peer, protocol::Opcode::kAttachExecutor,
                                         std::bind(&Impl::HandleAttachExecutor, this, std::placeholders::_1));
        context_->RegisterRequestHandler(enet_peer, protocol::Opcode::kAttachNode,
                                         std::bind(&Impl::HandleAttachNode, this, std::placeholders::_1));
        context_->RegisterRequestHandler(enet_peer, protocol::Opcode::kAttachHandle,
                                         std::bind(&Impl::HandleAttachHandle, this, std::placeholders::_1));
    }

    bool HandleAttachProcess(Context::RequestContext& request_context) {
        ASBLog(INFO) << "New process attaching...";
        std::lock_guard<std::mutex> lg(mutex_);
        auto enet_peer = request_context.GetENetPeer();
        auto it = process_proxy_map_.find(enet_peer);
        if (it != process_proxy_map_.end()) {
            ASBLog(ERROR) << "Duplicated process attched.";
            request_context.SetResponse(Result::kExisted);
            return false;
        }
        protocol::Process protocol_process;
        if (!protocol_process.ParseFromZeroCopyStream(&request_context.GetPayload())) {
            ASBLog(ERROR) << "Failed to parse attach proces request.";
            request_context.SetResponse(Result::kDeserializeError);
            return false;
        }
        auto process_proxy_impl = std::make_shared<ProcessProxyImpl>(context_, enet_peer, std::move(protocol_process));
        auto connection = process_proxy_impl->OnDisconnected.connect([this, enet_peer] {
            std::lock_guard<std::mutex> lg(mutex_);
            ASBLog(INFO) << "Process " << enet_peer << " disconnected, removing...";
            auto it = process_proxy_map_.find(enet_peer);
            if (it != process_proxy_map_.end()) {
                auto process_proxy = it->second.first;
                process_proxy_map_.erase(it);
                ScopedUnlocker<std::mutex> unlocker(mutex_);
                on_process_removed_(process_proxy);
            }
        });
        process_proxy_map_.emplace(enet_peer, std::make_pair(process_proxy_impl, connection));
        protocol::AttachResponse protocol_attach_response;
        protocol_attach_response.set_is_activated(process_proxy_impl->IsActivated());
        protocol_attach_response.mutable_instance()->set_id(reinterpret_cast<uint64_t>(enet_peer));
        request_context.SetResponse(Result::kOk, &protocol_attach_response);
        ScopedUnlocker<std::mutex> unlocker(mutex_);
        on_process_added_(process_proxy_impl);
        ASBLog(INFO) << "New process attached: " << process_proxy_impl->GetName() << "[" << process_proxy_impl->GetPid() << "]";
        return true;
    }

    bool HandleAttachChannel(Context::RequestContext& request_context) {
        ASBLog(INFO) << "New channel attaching...";
        std::lock_guard<std::mutex> lg(mutex_);
        auto enet_peer = request_context.GetENetPeer();
        auto it = channel_proxy_map_.find(enet_peer);
        if (it != channel_proxy_map_.end()) {
            ASBLog(ERROR) << "Duplicated channel attached.";
            request_context.SetResponse(Result::kExisted);
            return false;
        }
        protocol::Channel protocol_channel;
        if (!protocol_channel.ParseFromZeroCopyStream(&request_context.GetPayload())) {
            ASBLog(ERROR) << "Failed to parse attach channel request.";
            request_context.SetResponse(Result::kDeserializeError);
            return false;
        }
        ENetPeer* process_enet_peer = reinterpret_cast<ENetPeer*>(protocol_channel.owner_process().id());
        if (process_enet_peer == nullptr) {
            ASBLog(ERROR) << "Attach channel with invalid process.";
            request_context.SetResponse(Result::kInvalidParameter);
            return false;
        }
        auto pit = process_proxy_map_.find(process_enet_peer);
        if (pit == process_proxy_map_.end()) {
            ASBLog(ERROR) << "Attach channel with unknown process.";
            request_context.SetResponse(Result::kInvalidParameter);
            return false;
        }
        auto channel_proxy_impl = std::make_shared<ChannelProxyImpl>(context_, enet_peer, std::move(protocol_channel));
        auto connection = channel_proxy_impl->OnDisconnected.connect([this, enet_peer] {
            std::lock_guard<std::mutex> lg(mutex_);
            ASBLog(INFO) << "Channel " << enet_peer << " disconnected, removing...";
            auto it = channel_proxy_map_.find(enet_peer);
            if (it != channel_proxy_map_.end()) {
                channel_proxy_map_.erase(it);
            }
        });
        channel_proxy_map_.emplace(enet_peer, std::make_pair(channel_proxy_impl, connection));
        protocol::AttachResponse protocol_attach_response;
        protocol_attach_response.set_is_activated(channel_proxy_impl->IsActivated());
        protocol_attach_response.mutable_instance()->set_id(reinterpret_cast<uint64_t>(enet_peer));
        request_context.SetResponse(Result::kOk, &protocol_attach_response);
        pit->second.first->AddChannelProxy(channel_proxy_impl);
        ASBLog(INFO) << "New channel attached: " << channel_proxy_impl->GetId();
        return true;
    }

    bool HandleAttachExecutor(Context::RequestContext& request_context) {
        ASBLog(INFO) << "New executor attaching...";
        std::lock_guard<std::mutex> lg(mutex_);
        auto enet_peer = request_context.GetENetPeer();
        auto it = executor_proxy_map_.find(enet_peer);
        if (it != executor_proxy_map_.end()) {
            ASBLog(ERROR) << "Duplicated executor attached.";
            request_context.SetResponse(Result::kExisted);
            return false;
        }
        protocol::Executor protocol_executor;
        if (!protocol_executor.ParseFromZeroCopyStream(&request_context.GetPayload())) {
            ASBLog(ERROR) << "Failed to parse attach executor request.";
            request_context.SetResponse(Result::kDeserializeError);
            return false;
        }
        auto id = protocol_executor.owner_process().id();
        if (id == 0) {
            ASBLog(ERROR) << "Attach executor with invalid process id.";
            request_context.SetResponse(Result::kInvalidParameter);
            return false;
        }
        ENetPeer* process_enet_peer = reinterpret_cast<ENetPeer*>(protocol_executor.owner_process().id());
        if (process_enet_peer == nullptr) {
            ASBLog(ERROR) << "Attach executor with invalid process.";
            request_context.SetResponse(Result::kInvalidParameter);
            return false;
        }
        auto pit = process_proxy_map_.find(process_enet_peer);
        if (pit == process_proxy_map_.end()) {
            ASBLog(ERROR) << "Attach executor with unknown process.";
            request_context.SetResponse(Result::kInvalidParameter);
            return false;
        }
        auto executor_proxy_impl = std::make_shared<ExecutorProxyImpl>(context_, enet_peer, std::move(protocol_executor));
        auto connection = executor_proxy_impl->OnDisconnected.connect([this, enet_peer] {
            ASBLog(INFO) << "Executor " << enet_peer << " disconnected, removing...";
            std::lock_guard<std::mutex> lg(mutex_);
            auto it = executor_proxy_map_.find(enet_peer);
            if (it != executor_proxy_map_.end()) {
                executor_proxy_map_.erase(it);
            }
        });
        executor_proxy_map_.emplace(enet_peer, std::make_pair(executor_proxy_impl, connection));
        protocol::AttachResponse protocol_attach_response;
        protocol_attach_response.set_is_activated(executor_proxy_impl->IsActivated());
        protocol_attach_response.mutable_instance()->set_id(reinterpret_cast<uint64_t>(enet_peer));
        request_context.SetResponse(Result::kOk, &protocol_attach_response);
        pit->second.first->AddExecutorProxy(executor_proxy_impl);
        ASBLog(INFO) << "New executor attached.";
        return true;
    }

    bool HandleAttachNode(Context::RequestContext& request_context) {
        ASBLog(INFO) << "New node attaching...";
        std::lock_guard<std::mutex> lg(mutex_);
        auto enet_peer = request_context.GetENetPeer();
        auto it = node_proxy_map_.find(enet_peer);
        if (it != node_proxy_map_.end()) {
            ASBLog(ERROR) << "Duplicated node attached.";
            request_context.SetResponse(Result::kExisted);
            return false;
        }
        protocol::Node protocol_node;
        if (!protocol_node.ParseFromZeroCopyStream(&request_context.GetPayload())) {
            ASBLog(ERROR) << "Failed to parse attach node request.";
            request_context.SetResponse(Result::kDeserializeError);
            return false;
        }
        ENetPeer* process_enet_peer = reinterpret_cast<ENetPeer*>(protocol_node.owner_process().id());
        if (process_enet_peer == nullptr) {
            ASBLog(ERROR) << "Attach node with invalid process.";
            request_context.SetResponse(Result::kInvalidParameter);
            return false;
        }
        auto pit = process_proxy_map_.find(process_enet_peer);
        if (pit == process_proxy_map_.end()) {
            ASBLog(ERROR) << "Attach node with unknown process.";
            request_context.SetResponse(Result::kInvalidParameter);
            return false;
        }
        auto node_proxy_impl = std::make_shared<NodeProxyImpl>(context_, enet_peer, std::move(protocol_node));
        auto connection = node_proxy_impl->OnDisconnected.connect([this, enet_peer] {
            ASBLog(INFO) << "Node " << enet_peer << " disconnected, removing...";
            std::lock_guard<std::mutex> lg(mutex_);
            auto it = node_proxy_map_.find(enet_peer);
            if (it != node_proxy_map_.end()) {
                node_proxy_map_.erase(it);
            }
        });
        node_proxy_map_.emplace(enet_peer, std::make_pair(node_proxy_impl, connection));
        protocol::AttachResponse protocol_attach_response;
        protocol_attach_response.set_is_activated(node_proxy_impl->IsActivated());
        protocol_attach_response.mutable_instance()->set_id(reinterpret_cast<uint64_t>(enet_peer));
        request_context.SetResponse(Result::kOk, &protocol_attach_response);
        pit->second.first->AddNodeProxy(node_proxy_impl);
        ASBLog(INFO) << "New node attached: " << node_proxy_impl->GetName();
        return true;
    }

    bool HandleAttachHandle(Context::RequestContext& request_context) {
        ASBLog(INFO) << "New handle attaching...";
        std::lock_guard<std::mutex> lg(mutex_);
        auto enet_peer = request_context.GetENetPeer();
        auto it = handle_proxy_map_.find(enet_peer);
        if (it != handle_proxy_map_.end()) {
            ASBLog(ERROR) << "Duplicated handle attached.";
            request_context.SetResponse(Result::kExisted);
            return false;
        }
        protocol::Handle protocol_handle;
        if (!protocol_handle.ParseFromZeroCopyStream(&request_context.GetPayload())) {
            ASBLog(ERROR) << "Failed to parse attach handle request.";
            request_context.SetResponse(Result::kDeserializeError);
            return false;
        }
        ENetPeer* node_enet_peer = reinterpret_cast<ENetPeer*>(protocol_handle.owner_node().id());
        if (node_enet_peer == nullptr) {
            ASBLog(ERROR) << "Attach handle with invalid node.";
            request_context.SetResponse(Result::kInvalidParameter);
            return false;
        }
        auto nit = node_proxy_map_.find(node_enet_peer);
        if (nit == node_proxy_map_.end()) {
            ASBLog(ERROR) << "Attach handle with unknown node.";
            request_context.SetResponse(Result::kInvalidParameter);
            return false;
        }
        auto handle_proxy_impl = std::make_shared<HandleProxyImpl>(context_, enet_peer, std::move(protocol_handle));
        auto connection = handle_proxy_impl->OnDisconnected.connect([this, enet_peer] {
            ASBLog(INFO) << "Handle " << enet_peer << " disconnected, removing...";
            std::lock_guard<std::mutex> lg(mutex_);
            auto it = handle_proxy_map_.find(enet_peer);
            if (it != handle_proxy_map_.end()) {
                handle_proxy_map_.erase(it);
            }
        });
        handle_proxy_map_.emplace(enet_peer, std::make_pair(handle_proxy_impl, connection));
        protocol::AttachResponse protocol_attach_response;
        protocol_attach_response.set_is_activated(handle_proxy_impl->IsActivated());
        protocol_attach_response.mutable_instance()->set_id(reinterpret_cast<uint64_t>(enet_peer));
        request_context.SetResponse(Result::kOk, &protocol_attach_response);
        nit->second.first->AddHandleProxy(handle_proxy_impl);
        ASBLog(INFO) << "New handle attached: " << HandleTypeName(handle_proxy_impl->GetType()) << ", [" << handle_proxy_impl->GetKey() << "]";
        return true;
    }

private:
    using ProcessInfo = std::pair<std::shared_ptr<ProcessProxyImpl>, scoped_connection>;
    using ProcessProxyMap = std::map<ENetPeer*, ProcessInfo>;

    using ChannelInfo = std::pair<std::shared_ptr<ChannelProxyImpl>, scoped_connection>;
    using ChannelProxyMap = std::map<ENetPeer*, ChannelInfo>;

    using ExecutorInfo = std::pair<std::shared_ptr<ExecutorProxyImpl>, scoped_connection>;
    using ExecutorProxyMap = std::map<ENetPeer*, ExecutorInfo>;

    using NodeInfo = std::pair<std::shared_ptr<NodeProxyImpl>, scoped_connection>;
    using NodeProxyMap = std::map<ENetPeer*, NodeInfo>;

    using HandleInfo = std::pair<std::shared_ptr<HandleProxyImpl>, scoped_connection>;
    using HandleProxyMap = std::map<ENetPeer*, HandleInfo>;

 private:
    mutable std::mutex mutex_;
    signal<std::shared_ptr<ProcessProxy>>& on_process_added_;
    signal<std::shared_ptr<ProcessProxy>>& on_process_removed_;
    std::shared_ptr<Context> context_;
    std::string host_;
    uint16_t port_;
    ProcessProxyMap process_proxy_map_;
    ChannelProxyMap channel_proxy_map_;
    ExecutorProxyMap executor_proxy_map_;
    NodeProxyMap node_proxy_map_;
    HandleProxyMap handle_proxy_map_;
};

// Server

Server::Server(const std::string& host, uint16_t port)
    : impl_(std::make_shared<Impl>(host, port, OnProcessAdded, OnProcessRemoved)) {
}

Server::~Server() {
}

bool Server::Start() {
    return impl_->Start();
}

void Server::Stop() {
    impl_->Stop();
}

std::vector<std::shared_ptr<ProcessProxy>> Server::GetProcesses() const {
    return impl_->GetProcesses();
}

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf
