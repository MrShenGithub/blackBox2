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

#ifndef SF_MSGBUS_BLACKBOX2_CHANNEL_STUB_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_CHANNEL_STUB_IMPL_H_

#include <string>

#include <sf-msgbus/common/configuration.h>
#include <sf-msgbus/blackbox2/channel_stub.h>

#include "message_stub_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class ChannelStubImpl: public MessageStubImpl<ChannelStub> {
 public:
    ChannelStubImpl(std::shared_ptr<Context> context, const ChannelConfig& channel_config,
                    Handler inject_message_handler, Stub* process = nullptr);
    ~ChannelStubImpl() override;

 protected:
    void HandleParentInstanceIdChanged(uint64_t id) override;

 private:
    protocol::Channel protocol_channel_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_CHANNEL_STUB_IMPL_H_
