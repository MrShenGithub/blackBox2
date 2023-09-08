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

#ifndef SF_MSGBUS_BLACKBOX2_CHANNEL_PROXY_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_CHANNEL_PROXY_IMPL_H_

#include <string>

#include <sf-msgbus/blackbox2/channel_proxy.h>

#include "message_proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class ChannelProxyImpl: public MessageProxyImpl<ChannelProxy> {
public:
    ChannelProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer, protocol::Channel&& protocol_channel);
    ~ChannelProxyImpl() override;

public:
    int GetThreadId() const override;
    const std::string& GetThreadName() const override;
    const std::string& GetId() const override;
    const std::string& GetType() const override;
    const Properties& GetProperties() const override;
    bool IsCompatibleWith(const Properties& properties) const override;

private:
    protocol::Channel protocol_channel_;
    Properties properties_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_CHANNEL_PROXY_IMPL_H_
