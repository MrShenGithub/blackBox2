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

#ifndef SF_MSGBUS_BLACKBOX2_HANDLE_PROXY_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_HANDLE_PROXY_IMPL_H_

#include <sf-msgbus/types/result.h>
#include <sf-msgbus/blackbox2/handle_proxy.h>

#include "message_proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class HandleProxyImpl: public MessageProxyImpl<HandleProxy> {
 public:
    HandleProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer, protocol::Handle&& protocol_handle);
    ~HandleProxyImpl() override;

 public:
    int GetThreadId() const override;
    const std::string& GetThreadName() const override;
    HandleType GetType() const override;
    const std::string& GetKey() const override;
    bool IsEnabled() const override;
    const std::map<std::string, std::string>& GetMappingChannels() const override;

 private:
    void HandleEnable(google::protobuf::io::ZeroCopyInputStream& input);
    void HandleDisable(google::protobuf::io::ZeroCopyInputStream& input);

 private:
    protocol::Handle protocol_handle_;
    std::map<std::string, std::string> mapping_channels_;
    HandleType type_;
    bool is_enabled_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_HANDLE_PROXY_IMPL_H_
