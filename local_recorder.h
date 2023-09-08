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

#ifndef SF_MSGBUS_BLACKBOX2_LOCAL_RECORDER_H_
#define SF_MSGBUS_BLACKBOX2_LOCAL_RECORDER_H_

#include <string>
#include <thread>

#include <sf-msgbus/blackbox2/common.h>
#include <sf-msgbus/blackbox2/message.h>

namespace asf {
namespace msgbus {
namespace blackbox2 {

class LocalRecorder {
 public:
    LocalRecorder();

 public:
    bool Start(const std::string& path = "");
    void Stop();
    bool IsStarted() const;
    bool RecordMessage(Message&& message);
    bool RecordMessage(const Message& message);

 private:
    void RecordThread();

 private:
    std::thread record_thread_;
};

}
}
}

#endif //  SF_MSGBUS_BLACKBOX2_LOCAL_RECORDER_H_
