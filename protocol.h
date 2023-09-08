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

#ifndef SF_MSGBUS_BLACKBOX2_PROTOCOL_H_
#define SF_MSGBUS_BLACKBOX2_PROTOCOL_H_

#include <string>
#include <cstddef>
#include <cstdint>

#include <sf-msgbus/config.h>
#include <sf-msgbus/types/result.h>
#include <sf-msgbus/blackbox2/common.h>
#include <sf-msgbus/blackbox2/message.h>

#include "enet.h"
#include "protocol_message.pb.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

namespace protocol {
    enum class Type: uint8_t {
        kEvent = 0,
        kRequest,
        kResponse,
        kMax,
        kInvalid = 0xFFU
    };

    enum class Opcode: uint8_t {
        kActivate = 0,

        kAttachProcess,
        kAttachChannel,
        kAttachExecutor,
        kAttachNode,
        kAttachHandle,

        kMessage,
        kMessageFields,

        kProcessGetKeyStat,
        kProcessStartLocalPlayer,
        kProcessStopLocalPlayer,
        kProcessStartLocalRecorder,
        kProcessStopLocalRecorder,

        kExecutorAttachNode,
        kExecutorDetachNode,
        kExecutorRunBegin,
        kExecutorRunEnd,
        kExecutorTaskBegin,
        kExecutorTaskEnd,

        kNodeAttach,
        kNodeDetach,

        kHandleEnable,
        kHandleDisable,

        kMax,
        kInvalid = 0xFFU
    };

    constexpr uint8_t kVersion = 3;

    struct Header {
        uint8_t version;
        uint8_t type;
        uint8_t opcode;
        uint8_t __pad;
        uint32_t session;
        uint32_t extra_data;
    };

    void GetCurrentProcess(Process& out);
    void GetCurrentThread(Thread& out);
}

bool MessageFromProtocol(Message& out, std::shared_ptr<protocol::Message> in);

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_PROTOCOL_H_
