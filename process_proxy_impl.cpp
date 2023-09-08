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

#include "process_proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

ProcessProxyImpl::ProcessProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer, protocol::Process&& protocol_process)
    : ProxyImpl<ProcessProxy>(context, enet_peer)
    , protocol_process_(std::move(protocol_process)) {
    startup_time_ = std::chrono::system_clock::time_point(std::chrono::microseconds(protocol_process_.startup_timestamp()));
}

ProcessProxyImpl::~ProcessProxyImpl() {
}

int ProcessProxyImpl::GetThreadId() const {
    return protocol_process_.pid();
}

const std::string& ProcessProxyImpl::GetThreadName() const {
    return protocol_process_.name();
}

const std::string& ProcessProxyImpl::GetName() const {
    return protocol_process_.name();
}

const std::string& ProcessProxyImpl::GetCommandLine() const {
    return protocol_process_.cmdline();
}

const std::string& ProcessProxyImpl::GetWorkingDirectory() const {
    return protocol_process_.workding_directory();
}

const std::string& ProcessProxyImpl::GetEnvironments() const {
    return protocol_process_.environments();
}

const std::string& ProcessProxyImpl::GetConfigFilename() const {
    return protocol_process_.config_filename();
}

int ProcessProxyImpl::GetPid() const {
    return protocol_process_.pid();
}

std::chrono::system_clock::time_point ProcessProxyImpl::GetStartupTime() const {
    return startup_time_;
}

void ProcessProxyImpl::GetVersion(int& major, int& minor, int& patch) const {
    major = protocol_process_.version().major_number();
    minor = protocol_process_.version().minor_number();
    patch = protocol_process_.version().patch_number();
}

std::vector<std::shared_ptr<ChannelProxy>> ProcessProxyImpl::GetChannels() const {
    std::lock_guard<std::mutex> lg(GetMutex());
    std::vector<std::shared_ptr<ChannelProxy>> channels;
    channels.reserve(channel_proxy_vector_.size());
    for (auto& it: channel_proxy_vector_) {
        channels.push_back(it.first);
    }
    return channels;
}

std::vector<std::shared_ptr<ExecutorProxy>> ProcessProxyImpl::GetExecutors() const {
    std::lock_guard<std::mutex> lg(GetMutex());
    std::vector<std::shared_ptr<ExecutorProxy>> executors;
    executors.reserve(executor_proxy_vector_.size());
    for (auto& it: executor_proxy_vector_) {
        executors.push_back(it.first);
    }
    return executors;
}

std::vector<std::shared_ptr<NodeProxy>> ProcessProxyImpl::GetNodes() const {
    std::lock_guard<std::mutex> lg(GetMutex());
    std::vector<std::shared_ptr<NodeProxy>> nodes;
    nodes.reserve(node_proxy_vector_.size());
    for (auto& it: node_proxy_vector_) {
        nodes.push_back(it.first);
    }
    return nodes;
}

bool ProcessProxyImpl::GetKeyStat(const std::string& key, GetStatCallback cb) {
    std::lock_guard<std::mutex> lg(GetMutex());
    protocol::String protocol_key;
    protocol_key.set_value(key);
    return SendRequest(protocol::Opcode::kProcessGetKeyStat, protocol_key, [cb = std::move(cb)](Result result, google::protobuf::io::ZeroCopyInputStream* payload) {
        if (result != Result::kOk) {
            cb(result, nullptr);
            return;
        }
        protocol::KeyStat protocol_key_stat;
        if (!protocol_key_stat.ParseFromZeroCopyStream(payload)) {
            ASBLog(ERROR) << "Failed to parse handle stat response.";
            cb(Result::kDeserializeError, nullptr);
        }
        if (protocol_key_stat.valid()) {
            Stat handle_stat;
            handle_stat.rx_stat.rx_bytes = protocol_key_stat.rx_bytes();
            handle_stat.rx_stat.rx_length_errors = protocol_key_stat.rx_length_errors();
            handle_stat.rx_stat.rx_multicast = protocol_key_stat.rx_multicast();
            handle_stat.rx_stat.rx_no_buffer = protocol_key_stat.rx_no_buffer();
            handle_stat.rx_stat.rx_packets = protocol_key_stat.rx_packets();
            handle_stat.rx_stat.rx_subscriber = protocol_key_stat.rx_subscriber();
            handle_stat.rx_stat.rx_unsubscriber = protocol_key_stat.rx_unsubscriber();
            handle_stat.tx_stat.tx_bytes = protocol_key_stat.tx_bytes();
            handle_stat.tx_stat.tx_length_errors = protocol_key_stat.tx_length_errors();
            handle_stat.tx_stat.tx_multicast = protocol_key_stat.tx_multicast();
            handle_stat.tx_stat.tx_no_buffer = protocol_key_stat.tx_no_buffer();
            handle_stat.tx_stat.tx_no_channel = protocol_key_stat.tx_no_channel();
            handle_stat.tx_stat.tx_no_endpoint = protocol_key_stat.tx_no_endpoint();
            handle_stat.tx_stat.tx_no_subscriber = protocol_key_stat.tx_no_subscriber();
            handle_stat.tx_stat.tx_no_transmit = protocol_key_stat.tx_no_transmit();
            handle_stat.tx_stat.tx_packets = protocol_key_stat.tx_packets();
            handle_stat.tx_stat.tx_subscriber = protocol_key_stat.tx_subscriber();
            handle_stat.tx_stat.tx_unsubscriber = protocol_key_stat.tx_unsubscriber();
            cb(Result::kOk, &handle_stat);
        } else {
            cb(Result::kUnknown, nullptr);
        }
    });
}

void ProcessProxyImpl::AddChannelProxy(std::shared_ptr<ChannelProxyImpl> channel_proxy_impl) {
    std::lock_guard<std::mutex> lg(GetMutex());
    auto enet_peer = channel_proxy_impl->GetENetPeer();
    for (auto it = channel_proxy_vector_.begin(); it != channel_proxy_vector_.end(); ++it) {
        if (it->first->GetENetPeer() == enet_peer) {
            ASBLog(ERROR) << "Duplicated channel.";
            return;
        }
    }
    auto connection = channel_proxy_impl->OnDisconnected.connect([this, enet_peer] {
        std::lock_guard<std::mutex> lg(GetMutex());
        for (auto it = channel_proxy_vector_.begin(); it != channel_proxy_vector_.end(); ++it) {
            auto channel_proxy = it->first;
            if (channel_proxy->GetENetPeer() == enet_peer) {
                channel_proxy_vector_.erase(it);
                ScopedUnlocker<std::mutex> unlocker(GetMutex());
                OnChannelRemoved(channel_proxy);
                break;
            }
        }
    });
    channel_proxy_vector_.emplace_back(channel_proxy_impl, connection);
    ScopedUnlocker<std::mutex> unlocker(GetMutex());
    OnChannelAdded(channel_proxy_impl);
}

void ProcessProxyImpl::AddExecutorProxy(std::shared_ptr<ExecutorProxyImpl> executor_proxy_impl) {
    if (!executor_proxy_impl) {
        ASBLog(ERROR) << "Invalid executor proxy to add.";
        return;
    }
    std::lock_guard<std::mutex> lg(GetMutex());
    auto enet_peer = executor_proxy_impl->GetENetPeer();
    for (auto it = executor_proxy_vector_.begin(); it != executor_proxy_vector_.end(); ++it) {
        if (it->first->GetENetPeer() == enet_peer) {
            ASBLog(ERROR) << "Duplicated executor.";
            return;
        }
    }
    auto connection = executor_proxy_impl->OnDisconnected.connect([this, enet_peer] {
        std::lock_guard<std::mutex> lg(GetMutex());
        for (auto it = executor_proxy_vector_.begin(); it != executor_proxy_vector_.end(); ++it) {
            auto executor_proxy = it->first;
            if (executor_proxy->GetENetPeer() == enet_peer) {
                executor_proxy_vector_.erase(it);
                ScopedUnlocker<std::mutex> unlocker(GetMutex());
                OnExecutorRemoved(executor_proxy);
                break;
            }
        }
    });
    executor_proxy_vector_.emplace_back(executor_proxy_impl, connection);
    ScopedUnlocker<std::mutex> unlocker(GetMutex());
    OnExecutorAdded(executor_proxy_impl);
}

void ProcessProxyImpl::AddNodeProxy(std::shared_ptr<NodeProxyImpl> node_proxy_impl) {
    std::lock_guard<std::mutex> lg(GetMutex());
    auto enet_peer = node_proxy_impl->GetENetPeer();
    for (auto it = node_proxy_vector_.begin(); it != node_proxy_vector_.end(); ++it) {
        if (it->first->GetENetPeer() == enet_peer) {
            ASBLog(ERROR) << "Duplicated node.";
            return;
        }
    }
    auto connection = node_proxy_impl->OnDisconnected.connect([this, enet_peer] {
        std::lock_guard<std::mutex> lg(GetMutex());
        for (auto it = node_proxy_vector_.begin(); it != node_proxy_vector_.end(); ++it) {
            auto node_proxy = it->first;
            if (node_proxy->GetENetPeer() == enet_peer) {
                node_proxy_vector_.erase(it);
                ScopedUnlocker<std::mutex> unlocker(GetMutex());
                OnNodeRemoved(node_proxy);
            }
        }
    });
    node_proxy_vector_.emplace_back(node_proxy_impl, connection);
    ScopedUnlocker<std::mutex> unlocker(GetMutex());
    OnNodeAdded(node_proxy_impl);
}

void ProcessProxyImpl::HandleActivationChanged(bool is_activated) {
    for (auto it = channel_proxy_vector_.begin(); it != channel_proxy_vector_.end(); ++it) {
        it->first->SetActivation(is_activated);
    }
    for (auto it = executor_proxy_vector_.begin(); it != executor_proxy_vector_.end(); ++it) {
        it->first->SetActivation(is_activated);
    }
    for (auto it = node_proxy_vector_.begin(); it != node_proxy_vector_.end(); ++it) {
        it->first->SetActivation(is_activated);
    }
}

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf
