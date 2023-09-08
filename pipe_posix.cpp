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

#include <unistd.h>

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
        if (pipe_[0] >= 0) {
            close(pipe_[0]);
        }
        if (pipe_[1] >= 0) {
            close(pipe_[1]);
        }
    }

public:
    bool Open() {
        Close();
        return (pipe(pipe_) == 0);
    }

    void Close() {
        if (pipe_[0] >= 0) {
            close(pipe_[0]);
            pipe_[0] = -1;
        }
        if (pipe_[1] >= 0) {
            close(pipe_[1]);
            pipe_[1] = -1;
        }
    }

    bool IsOpen() const {
        return (pipe_[0] >= 0);
    }

    int64_t Read(void* buff, int64_t buff_size) {
        assert(pipe_[0] >= 0);
        return read(pipe_[0], buff, buff_size);
    }

    int64_t Write(const void* data, int64_t data_size) {
        assert(pipe_[1] >= 0);
        return write(pipe_[1], data, data_size);
    }

    int GetReadHandle() const {
        return pipe_[0];
    }

    int GetWriteHandle() const {
        return pipe_[1];
    }

private:
    int pipe_[2];
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
