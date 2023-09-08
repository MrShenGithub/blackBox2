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

#include "local_recorder.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

LocalRecorder::LocalRecorder() {
}

bool LocalRecorder::Start(const std::string& path) {
    return true;
}

void LocalRecorder::Stop() {
}

bool LocalRecorder::IsStarted() const {
    return false;
}

bool LocalRecorder::RecordMessage(Message&& message) {
    return false;
}

bool LocalRecorder::RecordMessage(const Message& message) {
    return false;
}

}
}
}
