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

#ifndef SF_MSGBUS_BLACKBOX2_EXECUTOR_STUB_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_EXECUTOR_STUB_IMPL_H_

#include <memory>
#include <string>

#include <sf-msgbus/blackbox2/executor_stub.h>

#include "stub_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class ExecutorStubImpl: public StubImpl<ExecutorStub> {
 public:
    ExecutorStubImpl(std::shared_ptr<Context> context, size_t thread_pool_size, Stub* process = nullptr);
    ~ExecutorStubImpl() override;

 public:
    void AttachNode(std::shared_ptr<NodeStub> node_stub) override;
    void DetachNode(std::shared_ptr<NodeStub> node_stub) override;
    void RunBegin() override;
    void RunEnd() override;
    void TaskBegin(int task_id) override;
    void TaskEnd(int task_id) override;

 public:
    struct ScopedRun final {
        ExecutorStub& r_;
        ScopedRun(ExecutorStub& r): r_(r) { r_.RunBegin(); }
        ~ScopedRun() { r_.RunEnd(); }
    };

    struct ScopedTask final {
        ExecutorStub& r_;
        int id_;
        ScopedTask(ExecutorStub& r, int id): r_(r), id_(id) { r_.TaskBegin(id_); }
        ~ScopedTask() { r_.TaskEnd(id_); }
    };

 protected:
    void HandleParentInstanceIdChanged(uint64_t id) override;

 private:
    protocol::Executor protocol_executor_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_EXECUTOR_STUB_IMPL_H_
