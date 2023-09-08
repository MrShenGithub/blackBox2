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

#if defined(__QNX__) || defined(__QNXNTO__)
    extern "C" char * __progname;
#   define CURRENT_PROCESS_NAME __progname
#   define CURRENT_THREAD_ID gettid()
#elif defined(__linux__)
#   ifndef _GNU_SOURCE
#       define _GNU_SOURCE
#   endif
#   include <errno.h>
#   include <unistd.h>
#   include <sys/syscall.h>
#   define CURRENT_PROCESS_NAME program_invocation_short_name
#   define CURRENT_THREAD_ID syscall(SYS_gettid)
#elif defined(_WIN32)
#   define CURRENT_PROCESS_NAME "<UnknownProcess>"
#   define CURRENT_THREAD_ID -1
#else
#   error unsupported platform.
#endif

#include <sf-msgbus/types/scope_guard.h>
#include <sf-msgbus/blackbox2/common.h>
#include <sf-msgbus/blackbox2/log.h>

#include "protocol.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

namespace protocol {

void GetCurrentProcess(Process& out) {
#ifdef _WIN32
    // TODO
#else
    out.set_pid(getpid());
    out.set_name(CURRENT_PROCESS_NAME);
    out.set_cmdline(CURRENT_PROCESS_NAME); // TODO

    char cwd[PATH_MAX + 1] = { 0 };
    getcwd(cwd, PATH_MAX);
    out.set_workding_directory(cwd);

#ifdef __linux
    auto* ep = out.mutable_environments();
    for (char**p = environ; *p != nullptr; ++p) {
        ep->append(";");
        ep->append(*p);
    }
#else
#   error Unknown platform.
#endif
#endif // _WIN32
}

void GetCurrentThread(Thread& out) {
#ifdef _WIN32
    // TODO
#else
    char name[256] = { 0 };
    pthread_getname_np(pthread_self(), name, 254);
    out.set_id(CURRENT_THREAD_ID);
    out.set_name(name);
#endif
}

}  // namespace protocol

bool MessageFromProtocol(Message& out, std::shared_ptr<protocol::Message> in) {
    assert(in);
    switch (in->dir()) {
    case protocol::Direction::In:
        out.SetDir(Message::Direction::kIn);
        break;
    case protocol::Direction::Out:
        out.SetDir(Message::Direction::kOut);
        break;
    default:
        out.SetDir(Message::Direction::kUnknown);
        break;
    }
    if (in->has_gen_timestamp()) {
        out.SetGenTimestamp(std::chrono::system_clock::time_point(std::chrono::microseconds(in->gen_timestamp())));
    }
    if (in->has_tx_timestamp()) {
        out.SetTxTimestamp(std::chrono::system_clock::time_point(std::chrono::microseconds(in->tx_timestamp())));
    }
    if (in->has_rx_timestamp()) {
        out.SetRxTimestamp(std::chrono::system_clock::time_point(std::chrono::microseconds(in->rx_timestamp())));
    }
    if (in->has_payload()) {
        if (!in->has_serialize_type()) {
            ASBLog(ERROR) << "Message has payload but no serialize type.";
            return false;
        }
        auto payload = in->payload();
        out.SetPayload(ByteArray(reinterpret_cast<uint8_t*>(const_cast<char*>(payload.data())),
                                 payload.length(), [in](uint8_t*, size_t, void*){}), in->serialize_type());
    }
    return true;
}

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf
