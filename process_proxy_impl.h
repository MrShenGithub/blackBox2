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

#ifndef SF_MSGBUS_BLACKBOX2_PROCESS_PROXY_IMPL_H_
#define SF_MSGBUS_BLACKBOX2_PROCESS_PROXY_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include <sf-msgbus/blackbox2/process_proxy.h>

#include "channel_proxy_impl.h"
#include "executor_proxy_impl.h"
#include "node_proxy_impl.h"
#include "proxy_impl.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

class ProcessProxyImpl: public ProxyImpl<ProcessProxy> {
 public:
    ProcessProxyImpl(std::shared_ptr<Context> context, ENetPeer* enet_peer, protocol::Process&& protocol_process);
    ~ProcessProxyImpl() override;

 public:
    int GetThreadId() const override;
    const std::string& GetThreadName() const override;
    const std::string& GetName() const override;
    const std::string& GetCommandLine() const override;
    const std::string& GetWorkingDirectory() const override;
    const std::string& GetEnvironments() const override;
    const std::string& GetConfigFilename() const override;
    int GetPid() const override;
    std::chrono::system_clock::time_point GetStartupTime() const override;
    void GetVersion(int& major, int& minor, int& patch) const override;
    std::vector<std::shared_ptr<ChannelProxy>> GetChannels() const override;
    std::vector<std::shared_ptr<ExecutorProxy>> GetExecutors() const override;
    std::vector<std::shared_ptr<NodeProxy>> GetNodes() const override;
    bool GetKeyStat(const std::string& key, GetStatCallback cb) override;

 public:
    void AddChannelProxy(std::shared_ptr<ChannelProxyImpl> channel_proxy_impl);
    void AddExecutorProxy(std::shared_ptr<ExecutorProxyImpl> executor_proxy_impl);
    void AddNodeProxy(std::shared_ptr<NodeProxyImpl> node_proxy_impl);

 protected:
    void HandleActivationChanged(bool is_activated) override;

 private:
    using ChannelInfo = std::pair<std::shared_ptr<ChannelProxyImpl>, scoped_connection>;
    using ChannelProxyVector = std::vector<ChannelInfo>;

    using ExecutorInfo = std::pair<std::shared_ptr<ExecutorProxyImpl>, scoped_connection>;
    using ExecutorProxyVector = std::vector<ExecutorInfo>;

    using NodeInfo = std::pair<std::shared_ptr<NodeProxyImpl>, scoped_connection>;
    using NodeProxyVector = std::vector<NodeInfo>;

 private:
    protocol::Process protocol_process_;
    std::vector<std::string> environments_;
    std::chrono::system_clock::time_point startup_time_;
    ChannelProxyVector channel_proxy_vector_;
    ExecutorProxyVector executor_proxy_vector_;
    NodeProxyVector node_proxy_vector_;
};

}  // namespace blackbox2
}  // namespace msgbus
}  // namespace asf

#endif  // SF_MSGBUS_BLACKBOX2_PROCESS_PROXY_IMPL_H_
