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

#include <list>
#include <memory>

#include <unistd.h>

#include <rapidjson/rapidjson.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/document.h>

#include <sf-msgbus/types/scope_guard.h>
#include <sf-msgbus/blackbox2/log.h>
#include <sf-msgbus/blackbox2/client.h>

#include "context.h"
#include "local_player.h"
#include "local_recorder.h"
#include "channel_stub_impl.h"
#include "executor_stub_impl.h"
#include "node_stub_impl.h"
#include "stub_impl.h"
#include "node/topic.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class Client::Impl final: public StubImpl<Stub> {
 public:
    Impl(std::shared_ptr<Context> context)
        : StubImpl<Stub>(context, protocol::Opcode::kAttachProcess, protocol_process_) {
        protocol::GetCurrentProcess(protocol_process_);
        protocol_process_.mutable_version()->set_major_number(SF_MSGBUS_VERSION_MAJOR);
        protocol_process_.mutable_version()->set_minor_number(SF_MSGBUS_VERSION_MINOR);
        protocol_process_.mutable_version()->set_patch_number(SF_MSGBUS_VERSION_PATCH);
        protocol_process_.set_startup_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        RegisterRequestHandler(protocol::Opcode::kProcessGetKeyStat, std::bind(&Impl::HandleGetKeyState, this, std::placeholders::_1));
        RegisterEventHandler(protocol::Opcode::kProcessStartLocalPlayer, std::bind(&Impl::HandleStartLocalPlayer, this, std::placeholders::_1));
        RegisterEventHandler(protocol::Opcode::kProcessStopLocalPlayer, std::bind(&Impl::HandleStopLocalPlayer, this, std::placeholders::_1));
        RegisterEventHandler(protocol::Opcode::kProcessStartLocalRecorder, std::bind(&Impl::HandleStartLocalRecorder, this, std::placeholders::_1));
        RegisterEventHandler(protocol::Opcode::kProcessStopLocalRecorder, std::bind(&Impl::HandleStopLocalRecorder, this, std::placeholders::_1));
        ASBLog(INFO) << this << ": client initialized.";
    }

    ~Impl() {
        ASBLog(INFO) << this << ": client released.";
    }

 public:
    void SetConfigFilename(const std::string& filename) {
        protocol_process_.set_config_filename(filename);
        protocol_process_.set_startup_timestamp(
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    std::shared_ptr<ChannelStub> CreateChannelStub(const ChannelConfig& channel_config,
                                                   ChannelStub::Handler inject_message_handler) {
        if (GetContext()->IsEnabled()) {
            auto channel_stub_impl = std::make_shared<ChannelStubImpl>(GetContext(), channel_config,
                                                                       std::move(inject_message_handler), this);
            if (channel_stub_impl && channel_stub_impl->Start()) {
                channels_.push_back(channel_stub_impl);
                return channel_stub_impl;
            }
        }
        return nullptr;
    }

    std::shared_ptr<ExecutorStub> CreateExecutorStub(size_t thread_pool_size) {
        if (GetContext()->IsEnabled()) {
            auto executor_stub_impl = std::make_shared<ExecutorStubImpl>(GetContext(), thread_pool_size, this);
            if (executor_stub_impl && executor_stub_impl->Start()) {
                return executor_stub_impl;
            }
        }
        return nullptr;
    }

    std::shared_ptr<NodeStub> CreateNodeStub(const std::string& name) {
        if (GetContext()->IsEnabled()) {
            auto node_stub_impl = std::make_shared<NodeStubImpl>(GetContext(), name, this);
            if (node_stub_impl && node_stub_impl->Start()) {
                return node_stub_impl;
            }
        }
        return nullptr;
    }

 private:
    void HandleStartLocalPlayer(google::protobuf::io::ZeroCopyInputStream& input) {
        std::lock_guard<std::mutex> lg(GetMutex());
        local_player_.Start();
        for (auto it = channels_.begin(); it != channels_.end(); ++it) {
            auto channel = it->lock();
            if (channel) {
                channel->SetLocalPlayer(&local_player_);
            } else {
                it = channels_.erase(it);
            }
        }
    }

    void HandleStopLocalPlayer(google::protobuf::io::ZeroCopyInputStream& input) {
        std::lock_guard<std::mutex> lg(GetMutex());
        for (auto it = channels_.begin(); it != channels_.end(); ++it) {
            auto channel = it->lock();
            if (channel) {
                channel->SetLocalPlayer(nullptr);
            } else {
                it = channels_.erase(it);
            }
        }
        local_recorder_.Stop();
    }

    void HandleStartLocalRecorder(google::protobuf::io::ZeroCopyInputStream& input) {
        std::lock_guard<std::mutex> lg(GetMutex());
        local_recorder_.Start();
        for (auto it = channels_.begin(); it != channels_.end(); ++it) {
            auto channel = it->lock();
            if (channel) {
                channel->SetLocalRecorder(&local_recorder_);
            } else {
                it = channels_.erase(it);
            }
        }
    }

    void HandleStopLocalRecorder(google::protobuf::io::ZeroCopyInputStream& input) {
        std::lock_guard<std::mutex> lg(GetMutex());
        for (auto it = channels_.begin(); it != channels_.end(); ++it) {
            auto channel = it->lock();
            if (channel) {
                channel->SetLocalRecorder(nullptr);
            } else {
                it = channels_.erase(it);
            }
        }
        local_recorder_.Stop();
    }

    void HandleGetKeyState(RequestContext& request_context) {
        protocol::KeyStat protocol_key_stat;
        protocol_key_stat.set_valid(false);
        protocol::String protocol_key;
        if (!protocol_key.ParseFromZeroCopyStream(&request_context.GetPayload())) {
            ASBLog(ERROR) << "Failed to parse get stat request.";
            request_context.SetResponse(Result::kDeserializeError);
            return;
        }
        TopicStatManager* ptsm = TopicStatManager::Instance();
        if (ptsm == nullptr) {
            ASBLog(ERROR) << "Failed to get topic stat manager for key " << protocol_key.value();
            request_context.SetResponse(Result::kInvalidState);
            return;
        }
        Stat* stat = ptsm->GetStat(protocol_key.value());
        if (stat != nullptr) {
            protocol_key_stat.set_valid(true);
            protocol_key_stat.set_rx_bytes(stat->rx_stat.rx_bytes);
            protocol_key_stat.set_rx_length_errors(stat->rx_stat.rx_length_errors);
            protocol_key_stat.set_rx_multicast(stat->rx_stat.rx_multicast);
            protocol_key_stat.set_rx_no_buffer(stat->rx_stat.rx_no_buffer);
            protocol_key_stat.set_rx_no_reader(stat->rx_stat.rx_no_reader);
            protocol_key_stat.set_rx_packets(stat->rx_stat.rx_packets);
            protocol_key_stat.set_rx_subscriber(stat->rx_stat.rx_subscriber);
            protocol_key_stat.set_rx_unsubscriber(stat->rx_stat.rx_unsubscriber);
            protocol_key_stat.set_tx_bytes(stat->tx_stat.tx_bytes);
            protocol_key_stat.set_tx_length_errors(stat->tx_stat.tx_length_errors);
            protocol_key_stat.set_tx_no_buffer(stat->tx_stat.tx_no_buffer);
            protocol_key_stat.set_tx_no_channel(stat->tx_stat.tx_no_channel);
            protocol_key_stat.set_tx_no_endpoint(stat->tx_stat.tx_no_endpoint);
            protocol_key_stat.set_tx_no_subscriber(stat->tx_stat.tx_no_subscriber);
            protocol_key_stat.set_tx_no_transmit(stat->tx_stat.tx_no_transmit);
            protocol_key_stat.set_tx_multicast(stat->tx_stat.tx_multicast);
            protocol_key_stat.set_tx_packets(stat->tx_stat.tx_packets);
            protocol_key_stat.set_tx_subscriber(stat->tx_stat.tx_subscriber);
            protocol_key_stat.set_tx_unsubscriber(stat->tx_stat.tx_unsubscriber);
            request_context.SetResponse(Result::kOk, &protocol_key_stat);
        } else {
            ASBLog(ERROR) << "Failed to get stat for key " << protocol_key.value();
            request_context.SetResponse(Result::kInvalidParameter);
        }
    }

 private:
    using Channels = std::list<std::weak_ptr<ChannelStubImpl>>;

 private:
    protocol::Process protocol_process_;
    LocalPlayer local_player_;
    LocalRecorder local_recorder_;
    Channels channels_;
};

Client::Client() {
    auto context = std::make_shared<Context>();
    if (context->StartAsClient()) {
        auto impl = std::make_shared<Impl>(context);
        if (impl && impl->Start()) {
            impl_ = impl;
        } else {
            throw std::system_error();
        }
    } else {
        ASBLog(ERROR) << "Failed to init client.";
    }
}

Client::~Client() {
}

Client& Client::GetInstance() {
    static Client instance;
    return instance;
}

void Client::SetConfigFilename(const std::string& filename) {
    if (impl_) {
        impl_->SetConfigFilename(filename);
    }
}

std::shared_ptr<ChannelStub> Client::CreateChannelStub(const ChannelConfig& channel_config, MessageStub::Handler inject_message_handler) {
    if (impl_) {
        return impl_->CreateChannelStub(channel_config, inject_message_handler);
    }
    return nullptr;
}

std::shared_ptr<ExecutorStub> Client::CreateExecutorStub(size_t thread_pool_size) {
    if (impl_) {
        return impl_->CreateExecutorStub(thread_pool_size);
    }
    return nullptr;
}

std::shared_ptr<NodeStub> Client::CreateNodeStub(const std::string& name) {
    if (impl_) {
        return impl_->CreateNodeStub(name);
    }
    return nullptr;
}

}
}
}
