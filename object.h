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

#ifndef SF_MSGBUS_BLACKBOX2_OBJECT_H_
#define SF_MSGBUS_BLACKBOX2_OBJECT_H_

#include <map>
#include <mutex>
#include <memory>
#include <type_traits>

#include <sf-msgbus/types/sigslot.h>
#include <sf-msgbus/types/scoped_unlocker.h>
#include <sf-msgbus/blackbox2/log.h>
#include <sf-msgbus/blackbox2/proxy.h>
#include <sf-msgbus/blackbox2/stub.h>

#include "context.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

template <typename T>
class Object: public T {
    static_assert(std::is_base_of<std::enable_shared_from_this<Proxy>, T>::value ||
                  std::is_base_of<std::enable_shared_from_this<Stub>, T>::value);

 public:
    Object(std::shared_ptr<Context> context, ENetPeer* enet_peer = nullptr)
        : context_(context)
        , enet_peer_(nullptr) {
        assert(context_);
        SetENetPeer(enet_peer);
    }

    virtual ~Object() {
        std::lock_guard<std::mutex> lg(mutex_);
        if (enet_peer_ != nullptr) {
            context_->UnregisterAll(enet_peer_);
            context_->Disconnect(enet_peer_, nullptr);
            SetENetPeer(nullptr);
        }
    }

 public:
    std::shared_ptr<Context> GetContext() {
        return context_;
    }

    ENetPeer* GetENetPeer() {
        return enet_peer_;
    }

 protected:
    std::mutex& GetMutex() const {
        return mutex_;
    }

    bool IsConnected() const {
        return (enet_peer_ != nullptr);
    }

    using ConnectCallback = std::function<void (Result)>;

    bool Connect(ConnectCallback cb) {
        if (enet_peer_ != nullptr) {
            return false;
        }
        auto wp = T::weak_from_this();
        bool ret = context_->Connect([this, cb = std::move(cb), wp](Result result, ENetPeer* enet_peer) {
            auto p = wp.lock();
            if (p) {
                {
                    std::lock_guard<std::mutex> lg(mutex_);
                    if (result == Result::kOk) {
                        SetENetPeer(enet_peer);
                    }
                }
                if (cb) {
                    cb(result);
                }
            } else {
                ASBLog(WARNING) << "Object " << this << " has been destroy.";
            }
        });
        return ret;
    }

    using DisconnectCallback = std::function<void (Result)>;

    bool Disconnect(DisconnectCallback cb = nullptr) {
        if (enet_peer_ == nullptr) {
            return false;
        }
        ENetPeer* enet_peer = enet_peer_;
        enet_peer_ = nullptr;
        auto wp = T::weak_from_this();
        return context_->Disconnect(enet_peer, [this, cb = std::move(cb), wp](Result result) {
            auto p = wp.lock();
            if (p) {
                if (cb) {
                    cb(result);
                }
            } else {
                ASBLog(WARNING) << "Object " << this << " has been destroy.";
            }
        });
    }

    bool SendEvent(protocol::Opcode opcode) {
        if (enet_peer_ == nullptr) {
            //ASBLog(ERROR) << this << ": failed to send packet, no connection.";
            return false;
        }
        return context_->SendEvent(enet_peer_, opcode, nullptr);
    }

    bool SendEvent(protocol::Opcode opcode, const google::protobuf::Message& payload) {
        if (enet_peer_ == nullptr) {
            //(ERROR) << this << ": failed to send packet, no connection.";
            return false;
        }
        return context_->SendEvent(enet_peer_, opcode, &payload);
    }

    using RequestContext = Context::RequestContext;
    using RequestCallback = Context::RequestCallback;

    bool SendRequest(protocol::Opcode opcode, RequestCallback cb) {
        if (enet_peer_ == nullptr) {
            //(ERROR) << this << ": failed to send packet, no connection.";
            return false;
        }
        auto wp = T::weak_from_this();
        return context_->SendRequest(enet_peer_, opcode, nullptr,
            [this, cb = std::move(cb), wp](Result result, google::protobuf::io::ZeroCopyInputStream* payload) {
                auto p = wp.lock();
                if (p) {
                    if (cb) {
                        cb(result, payload);
                    }
                } else {
                    ASBLog(WARNING) << "Object " << this << " has been destroy.";
                }
            });
    }

    bool SendRequest(protocol::Opcode opcode, const google::protobuf::Message& payload, RequestCallback cb) {
        if (enet_peer_ == nullptr) {
            //ASBLog(ERROR) << this << ": failed to send packet, no connection.";
            return false;
        }
        auto wp = T::weak_from_this();
        return context_->SendRequest(enet_peer_, opcode, &payload,
            [this, cb = std::move(cb), wp](Result result, google::protobuf::io::ZeroCopyInputStream* payload) {
                auto p = wp.lock();
                if (p) {
                    if (cb) {
                        cb(result, payload);
                    }
                } else {
                    ASBLog(WARNING) << "Object " << this << " has been destroy.";
                }
            });
    }

    using EventHandler = Context::EventHandler;

    void RegisterEventHandler(protocol::Opcode opcode, EventHandler handler) {
        if (enet_peer_ != nullptr) {
            context_->RegisterEventHandler(enet_peer_, opcode, handler);
        }
        if (!handler) {
            auto it = event_hander_map_.find(opcode);
            if (it != event_hander_map_.end()) {
                event_hander_map_.erase(it);
            }
        } else {
            event_hander_map_[opcode] = std::move(handler);
        }
    }

    using RequestHandler = Context::RequestHandler;

    void RegisterRequestHandler(protocol::Opcode opcode, RequestHandler handler) {
        if (enet_peer_ != nullptr) {
            context_->RegisterRequestHandler(enet_peer_, opcode, handler);
        }
        if (!handler) {
            auto it = request_handler_map_.find(opcode);
            if (it != request_handler_map_.end()) {
                request_handler_map_.erase(it);
            }
        } else {
            request_handler_map_[opcode] = std::move(handler);
        }
    }

 protected:
    virtual void HandleConnectionLost() {
    }

 private:
    void DisconnectHandler() {
        std::lock_guard<std::mutex> lg(mutex_);
        ASBLog(INFO) << this << ": disconnected.";
        SetENetPeer(nullptr);
        HandleConnectionLost();
    }

    void SetENetPeer(ENetPeer* enet_peer) {
        if (enet_peer_ == enet_peer) {
            return;
        }
        if (enet_peer_ != nullptr) {
            context_->UnregisterAll(enet_peer_);
        }
        enet_peer_ = enet_peer;
        if (enet_peer_ != nullptr) {
            context_->RegisterDisconnectHandler(enet_peer_, std::bind(&Object::DisconnectHandler, this));
            for (auto& h: event_hander_map_) {
                context_->RegisterEventHandler(enet_peer_, h.first, h.second);
            }
            for (auto& h: request_handler_map_) {
                context_->RegisterRequestHandler(enet_peer_, h.first, h.second);
            }
        }
    }

 private:
    using EventHandlerMap = std::map<protocol::Opcode, EventHandler>;
    using RequestHandlerMap = std::map<protocol::Opcode, RequestHandler>;

 private:
    mutable std::mutex mutex_;
    std::shared_ptr<Context> context_;
    ENetPeer* enet_peer_;
    EventHandlerMap event_hander_map_;
    RequestHandlerMap request_handler_map_;
};

struct DummyObjectT { };
using DummyObject = Object<DummyObjectT>;

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_OBJECT_H_
