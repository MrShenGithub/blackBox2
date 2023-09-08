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

#ifndef SF_MSGBUS_BLACKBOX2_NODE_STUB_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_NODE_STUB_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include <sf-msgbus/blackbox2/node_stub.h>

#include "stub_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class NodeStubImpl: public StubImpl<NodeStub> {
 public:
    NodeStubImpl(std::shared_ptr<Context> context, const std::string& name, Stub* process = nullptr);
    ~NodeStubImpl() override;

 public:
    const std::string& GetName() const override;
    void Attach() override;
    void Detach() override;
    std::shared_ptr<HandleStub> CreateHandleStub(HandleType handle_type, const std::string& key,
                                                 const std::map<std::string, std::string>& mapping_channels,
                                                 HandleStub::Handler inject_message_handler) override;

 protected:
    void HandleParentInstanceIdChanged(uint64_t id) override;

 private:
    protocol::Node protocol_node_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_NODE_STUB_IMPL_H_
