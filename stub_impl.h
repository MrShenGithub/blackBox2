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

#ifndef SF_MSGBUS_BLACKBOX2_STUB_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_STUB_IMPL_H_

#include <mutex>
#include <type_traits>

#include <sf-msgbus/types/sigslot.h>
#include <sf-msgbus/types/scoped_unlocker.h>
#include <sf-msgbus/blackbox2/stub.h>
#include <sf-msgbus/blackbox2/log.h>

#include "object.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

template <typename T>
class StubImpl: public Object<T> {
    static_assert(std::is_base_of<Stub, T>::value);

 public:
    StubImpl(std::shared_ptr<Context> context,
             protocol::Opcode attach_opcode,
             const google::protobuf::Message& attach_payload,
             Stub* parent = nullptr)
        : Object<T>(context)
        , attach_opcode_(attach_opcode)
        , attach_payload_(attach_payload)
        , is_activated_(true)
        , parent_(parent)
        , instance_id_(0) {
        Object<T>::RegisterEventHandler(protocol::Opcode::kActivate,
            std::bind(&StubImpl::ActivateHandler, this, std::placeholders::_1));
        if (parent_ != nullptr) {
            parent_instance_id_changed_connection_ = parent_->OnInstanceIdChanged.connect([this](uint64_t id) {
                std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
                HandleParentInstanceIdChanged(id);
                if (id > 0) {
                    ASBLog(INFO) << this << ": parent ready.";
                    TryToAttach();
                }
            });
        }
        connector_ = [this] {
            return Object<T>::Connect([this](Result result) {
                std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
                ConnectCallback(result);
            });
        };
    }

    ~StubImpl() override {
        ASBLog(INFO) << "Stub " << this << " releasing...\n";
        std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
        if (connector_) {
            connector_ = nullptr;
            SetInstanceId(0);
        } else {
            ASBLog(WARNING) << this << ": already stopped.";
        }
        ASBLog(INFO) << this << ": released.";
    }

 public:
    bool Start() {
        std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
        if (connector_) {
            connector_();
            return true;
        }
        return false;
    }

    Stub* GetParent() {
        return parent_;
    }

    uint64_t GetInstanceId() const override {
        return instance_id_;
    }

protected:
    bool IsActivated() const {
        return is_activated_;
    }

    bool SendEvent(protocol::Opcode opcode) {
        if (parent_ != nullptr && parent_->GetInstanceId() == 0) {
            //ASBLog(ERROR) << this << ": parent stub is not ready.";
            return false;
        }
        if (!is_activated_) {
            //ASBLog(ERROR) << this << ": Cannot send event " << static_cast<uint8_t>(opcode) << ", not activated.";
            return false;
        }
        return Object<T>::SendEvent(opcode);
    }

    bool SendEvent(protocol::Opcode opcode, const google::protobuf::Message& payload) {
        if (parent_ != nullptr && parent_->GetInstanceId() == 0) {
            //ASBLog(ERROR) << this << ": parent stub is not ready.";
            return false;
        }
        if (!is_activated_) {
            //ASBLog(ERROR) << this << ": Cannot send event " << static_cast<uint8_t>(opcode) << ", not activated.";
            return false;
        }
        return Object<T>::SendEvent(opcode, payload);
    }

    bool SendRequest(protocol::Opcode opcode, typename Object<T>::RequestCallback cb) {
        if (parent_ != nullptr && parent_->GetInstanceId() == 0) {
            //ASBLog(ERROR) << this << ": parent stub is not ready.";
            return false;
        }
        if (!is_activated_) {
            //ASBLog(ERROR) << this << ": Cannot send request " << static_cast<uint8_t>(opcode) << ", not activated.";
            return false;
        }
        return Object<T>::SendRequest(opcode, std::move(cb));
    }

    bool SendRequest(protocol::Opcode opcode, const google::protobuf::Message& payload, typename Object<T>::RequestCallback cb) {
        if (parent_ != nullptr && parent_->GetInstanceId() == 0) {
            //ASBLog(ERROR) << this << ": parent stub is not ready.";
            return false;
        }
        if (!is_activated_) {
            //ASBLog(ERROR) << this << ": Cannot send request " << static_cast<uint8_t>(opcode) << ", not activated.";
            return false;
        }
        return Object<T>::SendRequest(opcode, payload, std::move(cb));
    }

 protected:
    void HandleConnectionLost() override {
        ASBLog(INFO) << this << ": disconnected.";
        SetInstanceId(0);
        if (connector_) {
            ASBLog(INFO) << this << ": retry...";
            connector_();
        } else {
            ASBLog(INFO) << this << ": not started, no retry";
        }
    }

    virtual void HandleAttached() {
    }

    virtual void HandleParentInstanceIdChanged(uint64_t id) {
    }

 private:
    void SetInstanceId(uint64_t id) {
        ASBLog(INFO) << this << ": set instance id " << id;
        if (instance_id_ != id) {
            instance_id_ = id;
            ScopedUnlocker<std::mutex> unlocker(Object<T>::GetMutex());
            Stub::OnInstanceIdChanged(id);
        }
    }

    void ConnectCallback(Result result) {
        //ASBLog(INFO) << "Peer " << Object<T>::GetENetPeer() << " connect callback, result " << static_cast<int>(result);
        if (result != Result::kOk) {
            //ASBLog(ERROR) << this << ": connect failed: " << static_cast<int>(result);
            if (connector_) {
                //ASBLog(INFO) << this << ": retry...";
                connector_();
            } else {
                ASBLog(INFO) << this << ": not started.";
            }
        } else {
            TryToAttach();
        }
    }

    void TryToAttach() {
        if (parent_ != nullptr && parent_->GetInstanceId() == 0) {
            ASBLog(ERROR) << Object<T>::GetENetPeer() << ": attach: parent is not ready.";
            return;
        }
        if (instance_id_ > 0) {
            ASBLog(WARNING) << Object<T>::GetENetPeer() << ": attach: already attached.";
            return;
        }
        ASBLog(INFO) << Object<T>::GetENetPeer() << ": attaching...";
        bool ret = Object<T>::SendRequest(attach_opcode_, attach_payload_,
            std::bind(&StubImpl::HandleAttachResponse, this, std::placeholders::_1, std::placeholders::_2));
        if (!ret) {
            ASBLog(INFO) << Object<T>::GetENetPeer() << ": attach send failed.";
            Object<T>::Disconnect([this](Result) {
                std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
                if (connector_) {
                    connector_();
                }
            });
        }
    }

    void HandleAttachResponse(Result result, google::protobuf::io::ZeroCopyInputStream* payload) {
        ASBLog(INFO) << this << ": got attach response.";
        protocol::AttachResponse protocol_attach_response;
        if (protocol_attach_response.ParseFromZeroCopyStream(payload)) {
            ASBLog(INFO) << this << ": attached.";
            std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
            auto instance_id = protocol_attach_response.instance().id();
            if (instance_id > 0) {
                is_activated_ = protocol_attach_response.is_activated();
                SetInstanceId(instance_id);
                HandleAttached();
            } else {
                ASBLog(ERROR) << this << ": attach failed, disconnecting...";
                Object<T>::Disconnect([this](Result) {
                    ASBLog(ERROR) << this << ": reconnecting...";
                    std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
                    if (connector_) {
                        connector_();
                    }
                });
            }
        } else {
            ASBLog(ERROR) << this << ": failed to parse attach response.";
        }
    }

    void ActivateHandler(google::protobuf::io::ZeroCopyInputStream &input) {
        std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
        protocol::Boolean protocol_boolean;
        if (protocol_boolean.ParseFromZeroCopyStream(&input)) {
            is_activated_ = protocol_boolean.value();
        } else {
            ASBLog(ERROR) << "Failed to parse activation event.";
        }
    }

 private:
    protocol::Opcode attach_opcode_;
    const google::protobuf::Message& attach_payload_;
    Stub* parent_;
    std::function<bool ()> connector_;
    bool is_activated_;
    uint64_t instance_id_;
    scoped_connection parent_instance_id_changed_connection_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_STUB_IMPL_H_
