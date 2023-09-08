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

#ifndef SF_MSGBUS_BLACKBOX2_PROXY_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_PROXY_IMPL_H_

#include <mutex>
#include <memory>
#include <string>

#include <sf-msgbus/types/result.h>
#include <sf-msgbus/types/sigslot.h>
#include <sf-msgbus/types/scoped_unlocker.h>
#include <sf-msgbus/blackbox2/log.h>

#include "object.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

template <typename T>
class ProxyImpl: public Object<T> {
    static_assert(std::is_base_of<Proxy, T>::value);

 public:
    ProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer)
        : Object<T>(context, enet_peer)
        , is_activated_(true)
        , timestamp_(std::chrono::system_clock::now()) {
        assert(enet_peer != nullptr);
        char host_buf[512] = { 0 };
        enet_address_get_host(&enet_peer->address, host_buf, 510);
        host_ = host_buf;
        port_ = enet_peer->address.port;
    }

    ~ProxyImpl() override {
    }

 public:
    bool IsConnected() const override {
        std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
        return Object<T>::IsConnected();
    }

    bool Disconnect() override {
        std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
        Object<T>::Disconnect();
    }

    const std::string& GetHost() const override {
        return host_;
    }

    uint16_t GetPort() const override {
        return port_;
    }

    std::chrono::system_clock::time_point GetTimestamp() const override {
        return timestamp_;
    }

    bool IsActivated() const override {
        return is_activated_;
    }

    void SetActivation(bool v) override {
        std::lock_guard<std::mutex> lg(Object<T>::GetMutex());
        if (is_activated_ != v) {
            is_activated_ = v;
            protocol::Boolean protocol_boolean;
            protocol_boolean.set_value(v);
            Object<T>::SendEvent(protocol::Opcode::kActivate, protocol_boolean);
            ScopedUnlocker<std::mutex> unlocker(Object<T>::GetMutex());
            if (v) {
                Proxy::OnActivated();
            } else {
                Proxy::OnDeactivated();
            }
        }
    }

 protected:
    virtual void HandleActivationChanged(bool is_activated) {
    }

 protected:
    void HandleConnectionLost() override {
        ScopedUnlocker<std::mutex> unlocker(Object<T>::GetMutex());
        Proxy::OnDisconnected();
    }

 private:
    std::string host_;
    uint16_t port_;
    bool is_activated_;
    std::chrono::system_clock::time_point timestamp_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_PROXY_IMPL_H_
