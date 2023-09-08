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

#ifndef SF_MSGBUS_BLACKBOX2_PIPE_H_
#define SF_MSGBUS_BLACKBOX2_PIPE_H_

#include <memory>

#include <sf-msgbus/blackbox2/common.h>

namespace asf {
namespace msgbus {
namespace blackbox2 {

class Pipe {
 public:
    Pipe();

 public:
    bool Open();
    void Close();
    bool IsOpen() const;
    int64_t Read(void* buff, int64_t buff_size);
    int64_t Write(const void* data, int64_t data_size);
    int GetReadHandle() const;
    int GetWriteHandle() const;

 private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

}
}
}

#endif //  SF_MSGBUS_BLACKBOX2_PIPE_H_
