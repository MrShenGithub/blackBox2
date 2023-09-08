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

#ifndef SF_MSGBUS_BLACKBOX2_LOCAL_PLAYER_H_
#define SF_MSGBUS_BLACKBOX2_LOCAL_PLAYER_H_

#include <string>
#include <thread>

#include <sf-msgbus/blackbox2/common.h>
#include <sf-msgbus/blackbox2/message.h>

namespace asf {
namespace msgbus {
namespace blackbox2 {

class LocalPlayer {
 public:
    LocalPlayer();

 public:
    bool Start(const std::string& path = "");
    void Stop();
    bool IsStarted() const;

 private:
    void PlayThread();

 private:
    std::thread play_thread_;
};

}
}
}

#endif //  SF_MSGBUS_BLACKBOX2_LOCAL_PLAYER_H_
