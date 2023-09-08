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

#ifndef SF_MSGBUS_BLACKBOX2_MESSAGE_STUB_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_MESSAGE_STUB_IMPL_H_

#include <string>

#include <sf-msgbus/types/result.h>
#include <sf-msgbus/blackbox2/message_stub.h>

#include "local_player.h"
#include "local_recorder.h"
#include "stub_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

template <typename T>
class MessageStubImpl: public StubImpl<T> {
    static_assert(std::is_base_of<MessageStub, T>::value);

 public:
    MessageStubImpl(std::shared_ptr<Context> context, MessageStub::Handler inject_message_handler,
                    protocol::Opcode attach_opcode, const google::protobuf::Message& attach_payload, Stub* parent = nullptr)
        : StubImpl<T>(context, attach_opcode, attach_payload, parent)
        , inject_message_handler_(std::move(inject_message_handler))
        , message_fields_(Message::kHasDefault)
        , local_recorder_(nullptr) {
        Object<T>::RegisterEventHandler(protocol::Opcode::kMessage,
                                        std::bind(&MessageStubImpl::HandleMessage, this, std::placeholders::_1));
        Object<T>::RegisterEventHandler(protocol::Opcode::kMessageFields,
                                        std::bind(&MessageStubImpl::HandleMessageField, this, std::placeholders::_1));
    }

    ~MessageStubImpl() override {
        ASBLog(INFO) << "MessageStubImpl " << this << " releasing...";
    }

 public:
    void SendMessage(const ByteArray& payload, const std::string& serialize_type, const MessageInfo& msg_info) override {
        std::lock_guard<std::mutex> lg(StubImpl<T>::GetMutex());
        if (local_recorder_ != nullptr) {
            // TODO
        }
        if (message_fields_ == 0) {
            return;
        }
        if (!StubImpl<T>::IsActivated()) {
            return;
        }
        protocol::Message protocol_message;
        protocol_message.set_dir(protocol::Direction::Out);
        if ((message_fields_ & Message::kHasGenTimestamp) && msg_info.HasGenTimestamp()) {
            protocol_message.set_gen_timestamp(msg_info.GetGenTimestamp());
        }
        if ((message_fields_ & Message::kHasTxTimestamp) && msg_info.HasTxTimestamp()) {
            protocol_message.set_tx_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(msg_info.GetTxTimestamp().time_since_epoch()).count());
        }
        if ((message_fields_ & Message::kHasRxTimestamp) && msg_info.HasRxTimestamp()) {
            protocol_message.set_rx_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(msg_info.GetRxTimestamp().time_since_epoch()).count());
        }
        if ((message_fields_ & Message::kHasPayloadAndSerializeType)) {
            protocol_message.set_payload(std::string(reinterpret_cast<const char*>(payload.GetData()), payload.GetByteSize()));
            protocol_message.set_serialize_type(serialize_type);
        }
        StubImpl<T>::SendEvent(protocol::Opcode::kMessage, protocol_message);
    }

    void ReceiveMessage(const ByteArray& payload, const std::string& serialize_type, const MessageInfo& msg_info) override {
        std::lock_guard<std::mutex> lg(StubImpl<T>::GetMutex());
        if (local_recorder_ != nullptr) {
            // TODO
        }
        if (message_fields_ == 0) {
            return;
        }
        if (!StubImpl<T>::IsActivated()) {
            return;
        }
        protocol::Message protocol_message;
        protocol_message.set_dir(protocol::Direction::In);
        if ((message_fields_ & Message::kHasGenTimestamp) && msg_info.HasGenTimestamp()) {
            protocol_message.set_gen_timestamp(msg_info.GetGenTimestamp());
        }
        if ((message_fields_ & Message::kHasTxTimestamp) && msg_info.HasTxTimestamp()) {
            protocol_message.set_tx_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(msg_info.GetTxTimestamp().time_since_epoch()).count());
        }
        if ((message_fields_ & Message::kHasRxTimestamp) && msg_info.HasRxTimestamp()) {
            protocol_message.set_rx_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(msg_info.GetRxTimestamp().time_since_epoch()).count());
        }
        if ((message_fields_ & Message::kHasPayloadAndSerializeType)) {
            protocol_message.set_payload(std::string(reinterpret_cast<const char*>(payload.GetData()), payload.GetByteSize()));
            protocol_message.set_serialize_type(serialize_type);
        }
        StubImpl<T>::SendEvent(protocol::Opcode::kMessage, protocol_message);
    }

    void SetLocalPlayer(LocalPlayer* p) {
        std::lock_guard<std::mutex> lg(StubImpl<T>::GetMutex());
        local_player_ = p;
    }

    void SetLocalRecorder(LocalRecorder *p) {
        std::lock_guard<std::mutex> lg(StubImpl<T>::GetMutex());
        local_recorder_ = p;
    }

 private:
    void HandleMessage(google::protobuf::io::ZeroCopyInputStream& input) {
        if (!inject_message_handler_) {
            ASBLog(WARNING) << "No inject message handler.";
            return;
        }
        auto protocol_message = std::make_shared<protocol::Message>();
        if (!protocol_message->ParseFromZeroCopyStream(&input)) {
            ASBLog(ERROR) << "Failed to parse message event.";
            return;
        }
        Message message;
        if (!MessageFromProtocol(message, protocol_message)) {
            ASBLog(ERROR) << "Failed to decode message.";
            return;
        }
        ScopedUnlocker<std::mutex> unlocker(StubImpl<T>::GetMutex());
        inject_message_handler_(std::move(message));
    }

    void HandleMessageField(google::protobuf::io::ZeroCopyInputStream& input) {
        protocol::MessageFields protocol_message_fields;
        if (protocol_message_fields.ParseFromZeroCopyStream(&input)) {
            message_fields_ = protocol_message_fields.has_flags();
            ASBLog(INFO) << "Set message field " << message_fields_;
        } else {
            ASBLog(ERROR) << "Failed to parse message fields event.";
        }
    }

 private:
    MessageStub::Handler inject_message_handler_;
    unsigned int message_fields_;
    LocalPlayer* local_player_;
    LocalRecorder* local_recorder_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_HANDLE_STUB_IMPL_H_
