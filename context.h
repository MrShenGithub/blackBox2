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

#ifndef SF_MSGBUS_BLACKBOX2_CONTEXT_H_
#define SF_MSGBUS_BLACKBOX2_CONTEXT_H_

#include <map>
#include <mutex>
#include <memory>
#include <thread>
#include <functional>

#include <sf-msgbus/types/result.h>
#include <sf-msgbus/blackbox2/common.h>
#include <sf-msgbus/blackbox2/log.h>

#include "enet.h"
#include "pipe.h"
#include "protocol.h"

#ifdef _WIN32
#   undef GetCommandLine
#endif

namespace asf {
namespace msgbus {
namespace blackbox2 {

class Context: public std::enable_shared_from_this<Context> {
public:
    class RequestContext final {
    public:
        RequestContext(std::shared_ptr<Context> context, ENetPeer* enet_peer,
                       protocol::Opcode opcode, uint32_t session, google::protobuf::io::ZeroCopyInputStream& payload);
        ~RequestContext();

    public:
        std::shared_ptr<Context> GetContext();
        ENetPeer* GetENetPeer();
        protocol::Opcode GetOpcode() const;
        uint32_t GetSession() const;
        google::protobuf::io::ZeroCopyInputStream& GetPayload();
        bool SetResponse(Result result, const google::protobuf::Message* payload = nullptr);
        void FlushResponse();

    private:
        std::weak_ptr<Context> context_;
        ENetPeer* enet_peer_;
        protocol::Opcode opcode_;
        uint32_t session_;
        google::protobuf::io::ZeroCopyInputStream& payload_;
        bool response_dirty_;
        ENetPacket* response_packet_;
    };

public:
    using ConnectCallback = std::function<void (Result, ENetPeer*)>;
    using DisconnectCallback = std::function<void (Result)>;
    using ConnectHandler = std::function<void (ENetPeer*)>;
    using DisconnectHandler = std::function<void ()>;
    using EventHandler = std::function<void (google::protobuf::io::ZeroCopyInputStream&)>;
    using RequestHandler = std::function<void (RequestContext&)>;
    using RequestCallback = std::function<void (Result, google::protobuf::io::ZeroCopyInputStream*)>;

public:
    Context();
    Context(const Context&) = delete;
    Context(Context&&) = delete;
    virtual ~Context();

public:
    bool IsEnabled() const;
    bool StartAsClient(const ENetAddress* enet_address = nullptr);
    bool StartAsServer(ConnectHandler handler, const ENetAddress* enet_address = nullptr);
    void Stop();
    bool Connect(ConnectCallback cb);
    bool Disconnect(ENetPeer* enet_peer, DisconnectCallback cb = nullptr);
    bool SendEvent(ENetPeer* enet_peer, protocol::Opcode opcode, const google::protobuf::Message* payload);
    bool SendRequest(ENetPeer* enet_peer, protocol::Opcode opcode, const google::protobuf::Message* payload, RequestCallback cb);
    void RegisterDisconnectHandler(ENetPeer* enet_peer, DisconnectHandler handler);
    void RegisterEventHandler(ENetPeer* enet_peer, protocol::Opcode opcode, EventHandler handler);
    void RegisterRequestHandler(ENetPeer* enet_peer, protocol::Opcode opcode, RequestHandler handler);
    void UnregisterAll(ENetPeer* enet_peer);
    const ENetAddress& GetServerAddress();

private:
    enum AsyncCommand: char {
        kAsyncExit = 'x',
        kAsyncWakeup = 'w'
    };

    using ConnectCallbackMap = std::map<ENetPeer*, ConnectCallback>;
    using DisconnectCallbackMap = std::map<ENetPeer*, DisconnectCallback>;
    using DisconnectHandlerMap = std::map<ENetPeer*, DisconnectHandler>;
    using EventHandlerMap = std::map<ENetPeer*, std::map<protocol::Opcode, EventHandler>>;
    using RequestHandlerMap = std::map<ENetPeer*, std::map<protocol::Opcode, RequestHandler>>;
    using RequestSessionMap = std::map<ENetPeer*, std::map<uint32_t, RequestCallback>>;

    void InitConfig(const ENetAddress* enet_address);
    bool Start();
    bool WakeupBackend();
    void BackendThread();
    void HandleService();
    void HandleConnect(ENetPeer* enet_peer);
    void HandleDisconnect(ENetPeer* enet_peer);
    void HandlePacket(ENetPeer* enet_peer, ENetPacket* enet_pkt);
    void HandleEventPacket(ENetPeer* enet_peer, protocol::Opcode opcode, google::protobuf::io::ZeroCopyInputStream& payload);
    void HandleRequestPacket(ENetPeer* enet_peer, protocol::Opcode opcode, uint32_t session, google::protobuf::io::ZeroCopyInputStream& payload);
    void HandleResponsePacket(ENetPeer* enet_peer, uint32_t session, Result result, google::protobuf::io::ZeroCopyInputStream& payload);
    void HandleAsyncCommand();
    bool SendPacket(ENetPeer* enet_peer, ENetPacket* enet_pkt);
    bool SendPacket(ENetPeer* enet_peer, protocol::Type type, protocol::Opcode opcode, uint32_t session, const google::protobuf::Message* payload);
    static ENetPacket* CreateResponsePacket(protocol::Opcode opcode, uint32_t session, Result result, const google::protobuf::Message* payload = nullptr);
    static ENetPacket* CreatePacket(protocol::Type type, protocol::Opcode opcode, uint32_t session, const google::protobuf::Message* payload = nullptr, uint32_t extra_data = 0);

private:
    std::mutex mutex_;
    bool is_enabled_;
    ENetHost* enet_host_;
    ENetAddress server_address_;
    Pipe pipe_;
    bool backend_run_;
    std::thread backend_thread_;
    uint32_t session_;
    ConnectCallbackMap pending_connections_;
    DisconnectCallbackMap pending_disconnections_;
    ConnectHandler connect_handler_;
    DisconnectHandlerMap disconnect_handler_map_;
    EventHandlerMap event_handler_map_;
    RequestHandlerMap request_handler_map_;
    RequestSessionMap request_session_map_;
};

}
}
}

#endif  // SF_MSGBUS_BLACKBOX2_CONTEXT_H_
