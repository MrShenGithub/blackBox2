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

#include <cassert>

#include <winsock2.h>
#include <windows.h>

#include <sf-msgbus/types/scope_guard.h>

#include "pipe.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class Pipe::Impl final {
public:
    Impl() {
        pipe_[0] = -1;
        pipe_[1] = -1;
    }

    ~Impl() {
        // TODO release sockets.
    }

public:
    bool Open() {
        Close();
        SOCKET tcp1, tcp2;
        sockaddr_in name;
        memset(&name, 0, sizeof(name));
        name.sin_family = AF_INET;
        name.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        int namelen = sizeof(name);
        tcp1 = tcp2 = -1;
        SOCKET tcp = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp == -1) {
            return false;
        }
        auto tcp_guard = MakeScopeGuard([tcp] { closesocket(tcp); });
        if (bind(tcp, (sockaddr*)&name, namelen) == -1) {
            return false;
        }
        if (listen(tcp, 5) == -1) {
            return false;
        }
        if (getsockname(tcp, (sockaddr*)&name, &namelen) == -1) {
            return false;
        }
        tcp1 = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp1 == -1){
            return false;
        }
        auto tcp1_guard = MakeScopeGuard([tcp1] { closesocket(tcp1); });
        if (-1 == connect(tcp1, (sockaddr*)&name, namelen)) {
            return false;
        }
        tcp2 = accept(tcp, (sockaddr*)&name, &namelen);
        if (tcp2 == -1){
            return false;
        }
        pipe_[0] = tcp1;
        pipe_[1] = tcp2;
        tcp1_guard.Dismiss();
        return true;
    }

    void Close() {
        if (pipe_[0] >= 0) {
            closesocket(pipe_[0]);
            pipe_[0] = -1;
        }
        if (pipe_[1] >= 0) {
            closesocket(pipe_[1]);
            pipe_[1] = -1;
        }
    }

    bool IsOpen() const {
        return (pipe_[0] >= 0);
    }

    int64_t Read(void* buff, int64_t buff_size) {
        assert(pipe_[0] >= 0);
        return recv(pipe_[0], buff, buff_size, 0);
    }

    int64_t Write(const void* data, int64_t data_size) {
        assert(pipe_[1] >= 0);
        return send(pipe_[1], data, data_size, 0);
    }

    int GetReadHandle() const {
        return pipe_[0];
    }

    int GetWriteHandle() const {
        return pipe_[1];
    }

private:
    SOCKET pipe_[2];
};

Pipe::Pipe()
    : impl_(std::make_shared<Impl>()) {
}

bool Pipe::Open() {
    return impl_->Open();
}

void Pipe::Close() {
    impl_->Close();
}

bool Pipe::IsOpen() const {
    return impl_->IsOpen();
}

int64_t Pipe::Read(void *buff, int64_t buff_size) {
    return impl_->Read(buff, buff_size);
}

int64_t Pipe::Write(const void *data, int64_t data_size) {
    return impl_->Write(data, data_size);
}

int Pipe::GetReadHandle() const {
    return impl_->GetReadHandle();
}

int Pipe::GetWriteHandle() const {
    return impl_->GetWriteHandle();
}

}
}
}
