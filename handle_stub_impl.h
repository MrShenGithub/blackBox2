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

#ifndef SF_MSGBUS_BLACKBOX2_HANDLE_STUB_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_HANDLE_STUB_IMPL_H_

#include <map>
#include <string>

#include <sf-msgbus/types/result.h>
#include <sf-msgbus/message/message_info.h>
#include <sf-msgbus/blackbox2/channel_stub.h>
#include <sf-msgbus/blackbox2/handle_stub.h>

#include "message_stub_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class HandleStubImpl: public MessageStubImpl<HandleStub> {
 public:
    HandleStubImpl(std::shared_ptr<Context> context, HandleType handle_type, const std::string& key,
                   const std::map<std::string, std::string>& mapping_channels, Handler inject_message_handler,
                   Stub* node = nullptr);
    ~HandleStubImpl() override;

 public:
    void Enable() override;
    void Disable() override;

 protected:
    void HandleParentInstanceIdChanged(uint64_t id) override;

 private:
    protocol::Handle protocol_handle_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_HANDLE_STUB_IMPL_H_
