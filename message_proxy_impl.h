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

#ifndef SF_MSGBUS_BLACKBOX2_MESSAGE_PROXY_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_MESSAGE_PROXY_IMPL_H_

#include <sf-msgbus/blackbox2/message_proxy.h>

#include "proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

template <typename T>
class MessageProxyImpl: public ProxyImpl<T> {
    static_assert(std::is_base_of<MessageProxy, T>::value);

 public:
    MessageProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer)
        : ProxyImpl<T>(context, enet_peer)
        , message_fields_(Message::kHasDefault) {
        ProxyImpl<T>::RegisterEventHandler(protocol::Opcode::kMessage,
            std::bind(&MessageProxyImpl::HandleMessage, this, std::placeholders::_1));
    }

    ~MessageProxyImpl() override {
    }

 public:
    Result InjectMessage(Message&& message) override {
        if (!message.HasPayloadAndSerializeType()) {
            return Result::kInvalidParameter;
        }
        std::lock_guard<std::mutex> lg(ProxyImpl<T>::GetMutex());
        if (message_fields_ == 0) {
            return Result::kInvalidState;
        }
        protocol::Message protocol_message;
        switch (message.GetDir()) {
        case Message::Direction::kIn:
            protocol_message.set_dir(protocol::Direction::In);
            break;
        case Message::Direction::kOut:
            protocol_message.set_dir(protocol::Direction::Out);
            break;
        default:
            return Result::kInvalidParameter;
        }
        if (message.HasGenTimestamp()) {
            protocol_message.set_gen_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(message.GetGenTimestamp().time_since_epoch()).count());
        }
        if (message.HasTxTimestamp()) {
            protocol_message.set_gen_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(message.GetTxTimestamp().time_since_epoch()).count());
        }
        if (message.HasRxTimestamp()) {
            protocol_message.set_gen_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(message.GetRxTimestamp().time_since_epoch()).count());
        }
        const auto& payload = message.GetPayload();
        protocol_message.set_payload(std::string(reinterpret_cast<const char*>(payload.GetData()), payload.GetByteSize()));
        protocol_message.set_serialize_type(message.GetSerializeType());
        if (!ProxyImpl<T>::SendEvent(protocol::Opcode::kMessage, protocol_message)) {
            return Result::kUnknown;
        }
        return Result::kOk;
    }

    unsigned int GetMessageFields() const override {
        return message_fields_;
    }

    void SetMessageFields(unsigned int fields) override {
        std::lock_guard<std::mutex> lg(ProxyImpl<T>::GetMutex());
        if (message_fields_ != fields) {
            message_fields_ = fields;
            protocol::MessageFields protocol_message_fields;
            protocol_message_fields.set_has_flags(message_fields_);
            ProxyImpl<T>::SendEvent(protocol::Opcode::kMessageFields, protocol_message_fields);
        }
    }

 private:
    void HandleMessage(google::protobuf::io::ZeroCopyInputStream& input) {
        auto protocol_message = std::make_shared<protocol::Message>();
        if (!protocol_message->ParseFromZeroCopyStream(&input)) {
            ASBLog(ERROR) << "Failed to parse message packet.";
            return;
        }
        Message message;
        if (MessageFromProtocol(message, protocol_message)) {
            T::OnMessage(message);
        } else {
            ASBLog(ERROR) << "Failed to convert message.";
        }
    }

 private:
    unsigned int message_fields_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_HANDLE_PROXY_IMPL_H_
