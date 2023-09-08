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

#include <cerrno>
#include <cassert>
#include <cstring>

#if defined(__linux__)
#   include <pthread.h>
#   define SET_CURRENT_THREAD_NAME(n) pthread_setname_np(pthread_self(), n)
#else
#   define SET_CURRENT_THREAD_NAME(n) do { } while (0)
#endif

#include <sf-msgbus/types/scope_guard.h>
#include <sf-msgbus/types/scoped_unlocker.h>
#include <sf-msgbus/blackbox2/log.h>

#include "context.h"

namespace asf {
namespace msgbus {
namespace blackbox2 {

Context::RequestContext::RequestContext(std::shared_ptr<Context> context, ENetPeer* enet_peer,
                                        protocol::Opcode opcode, uint32_t session, google::protobuf::io::ZeroCopyInputStream& payload)
    : context_(context)
    , enet_peer_(enet_peer)
    , opcode_(opcode)
    , session_(session)
    , payload_(payload)
    , response_dirty_(true)
    , response_packet_(nullptr) {
}

Context::RequestContext::~RequestContext() {
    FlushResponse();
}

std::shared_ptr<Context> Context::RequestContext::GetContext() {
    return context_.lock();
}

ENetPeer* Context::RequestContext::GetENetPeer() {
    return enet_peer_;
}

protocol::Opcode Context::RequestContext::GetOpcode() const {
    return opcode_;
}

uint32_t Context::RequestContext::GetSession() const {
    return session_;
}

google::protobuf::io::ZeroCopyInputStream& Context::RequestContext::GetPayload() {
    return payload_;
}

bool Context::RequestContext::SetResponse(Result result, const google::protobuf::Message* payload) {
    if (response_packet_ != nullptr) {
        enet_packet_destroy(response_packet_);
    }
    response_packet_ = CreateResponsePacket(opcode_, session_, result, payload);
    response_dirty_ = (response_packet_ != nullptr);
    return response_dirty_;
}

void Context::RequestContext::FlushResponse() {
    if (response_dirty_) {
        if (response_packet_ == nullptr) {
            response_packet_ = CreateResponsePacket(opcode_, session_, Result::kUnknown);
        }
        if (response_packet_ != nullptr) {
            auto context = context_.lock();
            if (context) {
                if (context->SendPacket(enet_peer_, response_packet_)) {
                    response_dirty_ = false;
                } else {
                    enet_packet_destroy(response_packet_);
                }
            }
            response_packet_ = nullptr;
        }
    }
}

// Context

Context::Context()
    : is_enabled_(false)
    , enet_host_(nullptr)
    , backend_run_(false)
    , session_(1) {
}

Context::~Context() {
    Stop();
}

bool Context::IsEnabled() const {
    return is_enabled_;
}

bool Context::StartAsClient(const ENetAddress* enet_address) {
    std::unique_lock<std::mutex> lg(mutex_);

    InitConfig(enet_address);
    if (!is_enabled_) {
        ASBLog(ERROR) << "Blackbox2 not enabled.";
        return false;
    }

    if (backend_run_ && pipe_.IsOpen()) {
        ASBLog(ERROR) << "ENet host has already started.";
        return false;
    }

    assert(enet_host_ == nullptr);
    enet_host_ = enet_host_create(nullptr, 256, 1, 0, 0);
    if (enet_host_ == nullptr) {
        return false;
    }

    auto enet_host_guard = MakeScopeGuard([this] {
        enet_host_destroy(enet_host_);
        enet_host_ = nullptr;
    });

    if (!Start()) {
        return false;
    }

    enet_host_guard.Dismiss();

    return true;
}

bool Context::StartAsServer(ConnectHandler handler, const ENetAddress* enet_address) {
    std::unique_lock<std::mutex> lg(mutex_);

    if (backend_run_ && pipe_.IsOpen()) {
        ASBLog(ERROR) << "ENet host has already started.";
        return false;
    }

    InitConfig(enet_address);

    assert(enet_host_ == nullptr);
    enet_host_ = enet_host_create(&server_address_, 512, 1, 0, 0);
    if (enet_host_ == nullptr) {
        ASBLog(ERROR) << "Failed to create enet host.";
        return false;
    }

    auto enet_host_guard = MakeScopeGuard([this] {
        enet_host_destroy(enet_host_);
        enet_host_ = nullptr;
    });

    if (!Start()) {
        return false;
    }

    connect_handler_ = std::move(handler);
    WakeupBackend();
    enet_host_guard.Dismiss();

    return true;
}

void Context::Stop() {
    std::unique_lock<std::mutex> lg(mutex_);

    if (backend_run_) {
        assert(pipe_.IsOpen());
        char quit_cmd = AsyncCommand::kAsyncExit;
        if (pipe_.Write(&quit_cmd, 1) == 1) {
            ScopedUnlocker<std::mutex> unlocker(mutex_);
            backend_thread_.join();
        } else {
            ASBLog(ERROR) << "Failed to write enet async pipe: " << strerror(errno);
        }
    }

    pipe_.Close();

    if (enet_host_ != nullptr) {
        enet_host_destroy(enet_host_);
        enet_host_ = nullptr;
    }

    pending_connections_.clear();
    pending_disconnections_.clear();
    disconnect_handler_map_.clear();
    connect_handler_ = nullptr;
}

bool Context::Connect(ConnectCallback cb) {
    std::unique_lock<std::mutex> lg(mutex_);

    if (enet_host_ == nullptr) {
        ASBLog(ERROR) << "Connect with invalid enet host.";
        return false;
    }

    ENetPeer* enet_peer = enet_host_connect(enet_host_, &server_address_, 1, 0);
    if (enet_peer == nullptr) {
        ASBLog(ERROR) << "Connect failed.";
        return false;
    }

    //ASBLog(INFO) << "Peer " << enet_peer << " connecting...";

    enet_peer_timeout(enet_peer, 3, 1000, 4000);
    pending_connections_[enet_peer] = std::move(cb);
    WakeupBackend();

    return true;
}

bool Context::Disconnect(ENetPeer* enet_peer, DisconnectCallback cb) {
    assert(enet_peer != nullptr);
    std::unique_lock<std::mutex> lg(mutex_);
    if (enet_host_ == nullptr || enet_peer == nullptr) {
        ASBLog(ERROR) << "Disconnect with invalid enet_host or invalid enet_peer.";
        return false;
    }
    if (pending_disconnections_.count(enet_peer) == 0) {
        enet_peer_disconnect(enet_peer, 0);
        if (cb) {
            pending_disconnections_[enet_peer] = std::move(cb);
        }
        WakeupBackend();
    } else {
        ASBLog(WARNING) << "Peer " << enet_peer << " is in disconnecting.";
    }
    return true;
}

bool Context::SendEvent(ENetPeer* enet_peer, protocol::Opcode opcode, const google::protobuf::Message* payload) {
    assert(enet_peer != nullptr);
    std::lock_guard<std::mutex> lg(mutex_);
    return SendPacket(enet_peer, protocol::Type::kEvent, opcode, 0, payload);
}

bool Context::SendRequest(ENetPeer* enet_peer, protocol::Opcode opcode, const google::protobuf::Message* payload, RequestCallback cb) {
    assert(enet_peer != nullptr);
    assert(cb);
    std::lock_guard<std::mutex> lg(mutex_);
    if (SendPacket(enet_peer, protocol::Type::kRequest, opcode, session_, payload)) {
        //ASBLog(INFO) << "Send request: opcode " << static_cast<uint8_t>(opcode) << ", session " << session_;
        request_session_map_[enet_peer][session_] = std::move(cb);
        session_ += 1;
        return true;
    }
    return false;
}

void Context::RegisterDisconnectHandler(ENetPeer* enet_peer, DisconnectHandler handler) {
    assert(enet_peer != nullptr);
    assert(handler);
    std::unique_lock<std::mutex> lg(mutex_);
    if (!handler) {
        auto it = disconnect_handler_map_.find(enet_peer);
        if (it != disconnect_handler_map_.end()) {
            disconnect_handler_map_.erase(it);
        }
    } else {
        disconnect_handler_map_[enet_peer] = std::move(handler);
    }
}

void Context::RegisterEventHandler(ENetPeer* enet_peer, protocol::Opcode opcode, EventHandler handler) {
    assert(enet_peer != nullptr);
    assert(handler);
    std::unique_lock<std::mutex> lg(mutex_);
    if (!handler) {
        auto pit = event_handler_map_.find(enet_peer);
        if (pit == event_handler_map_.end()) {
            return;
        }
        auto hit = pit->second.find(opcode);
        if (hit == pit->second.end()) {
            return;
        }
        pit->second.erase(hit);
        if (pit->second.empty()) {
            event_handler_map_.erase(pit);
        }
    } else {
        event_handler_map_[enet_peer][opcode] = std::move(handler);
    }
}

void Context::RegisterRequestHandler(ENetPeer* enet_peer, protocol::Opcode opcode, RequestHandler handler) {
    assert(enet_peer != nullptr);
    assert(handler);
    std::unique_lock<std::mutex> lg(mutex_);
    if (!handler) {
        auto pit = request_handler_map_.find(enet_peer);
        if (pit == request_handler_map_.end()) {
            return;
        }
        auto hit = pit->second.find(opcode);
        if (hit == pit->second.end()) {
            return;
        }
        pit->second.erase(hit);
        if (pit->second.empty()) {
            request_handler_map_.erase(pit);
        }
    } else {
        request_handler_map_[enet_peer][opcode] = std::move(handler);
    }
}

void Context::UnregisterAll(ENetPeer* enet_peer) {
    assert(enet_peer != nullptr);
    ASBLog(INFO) << "Peer " << enet_peer << " unregister all...";
    std::unique_lock<std::mutex> lg(mutex_);
    auto ehit = event_handler_map_.find(enet_peer);
    if (ehit != event_handler_map_.end()) {
        event_handler_map_.erase(ehit);
    }
    auto rhit = request_handler_map_.find(enet_peer);
    if (rhit != request_handler_map_.end()) {
        request_handler_map_.erase(rhit);
    }
    auto rsit = request_session_map_.find(enet_peer);
    if (rsit != request_session_map_.end()) {
        request_session_map_.erase(rsit);
    }
    auto pcit = pending_connections_.find(enet_peer);
    if (pcit != pending_connections_.end()) {
        pending_connections_.erase(pcit);
    }
    auto pdit = pending_disconnections_.find(enet_peer);
    if (pdit != pending_disconnections_.end()) {
        pending_disconnections_.erase(pdit);
    }
    auto dhit = disconnect_handler_map_.find(enet_peer);
    if (dhit != disconnect_handler_map_.end()) {
        disconnect_handler_map_.erase(dhit);
    }
}

const ENetAddress& Context::GetServerAddress() {
    return server_address_;
}

void Context::InitConfig(const ENetAddress *enet_address) {
    const char* env_enable = getenv("SF_MSGBUS_BLACKBOX2_ENABLE");
    if (env_enable != nullptr && env_enable[0] == '1') {
        is_enabled_ = true;
        ASBLog(INFO) << "Blackbox2 is enabled.";
    }
    if (enet_address == nullptr) {
        const char *host = getenv("SF_MSGBUS_BLACKBOX2_HOST");
        if (host != nullptr) {
            enet_address_set_host(&server_address_, host);
        } else {
            enet_address_set_host(&server_address_, kDefaultHost);
        }
        const char *port = getenv("SF_MSGBUS_BLACKBOX2_PORT");
        if (port != nullptr) {
            server_address_.port = atoi(port);
        } else {
            server_address_.port = kDefaultPort;
        }
    } else {
        server_address_ = *enet_address;
    }
    char host_name[64] = { 0 };
    enet_address_get_host_new(&server_address_, host_name, 60);
    ASBLog(INFO) << "Blackbox2 server: " << host_name << ":" << server_address_.port;
}

bool Context::Start() {
    assert(!pipe_.IsOpen());
    if (!pipe_.Open()) {
        ASBLog(ERROR) << "Failed to create async pipe, " << strerror(errno);
        return false;
    }
    auto pipe_guard = MakeScopeGuard([this] {
        pipe_.Close();
    });
    backend_run_ = true;
    auto backend_run_guard = MakeScopeGuard([this] {
        backend_run_ = false;
    });
    backend_thread_ = std::thread(std::bind(&Context::BackendThread, this));
    pipe_guard.Dismiss();
    backend_run_guard.Dismiss();
    return true;
}

bool Context::WakeupBackend() {
    if (!backend_run_) {
        ASBLog(WARNING) << "Backend thread is not running.";
        return false;
    }
    assert(pipe_.IsOpen());
    char pipe_cmd = AsyncCommand::kAsyncWakeup;
    if (pipe_.Write(&pipe_cmd, 1) != 1) {
        ASBLog(ERROR) << "Failed to write async pipe.";
        return false;
    }
    return true;
}

void Context::BackendThread() {
    SET_CURRENT_THREAD_NAME("ENetThread");
    ASBLog(INFO) << "ENetThread start.";

    std::unique_lock<std::mutex> lg(mutex_);

    auto guard = MakeScopeGuard([] {
        ASBLog(INFO) << "Backend thread exited.";
    });

    assert(pipe_.IsOpen());

    int ret;
    int skmax;
    int pipe_rh = pipe_.GetReadHandle();
    ENetSocketSet skset;

    while (backend_run_) {
        ENET_SOCKETSET_EMPTY(skset);
        ENET_SOCKETSET_ADD(skset, pipe_rh);

        if (enet_host_ != nullptr) {
            HandleService();
        }

        if (enet_host_ != nullptr) {
            ENET_SOCKETSET_ADD(skset, enet_host_->socket);
            skmax = std::max(skmax, (int)(enet_host_->socket));
        } else {
            skmax = pipe_rh;
        }

        {
            ScopedUnlocker<std::mutex> unlocker(mutex_);
            ret = enet_socketset_select(skmax + 1, &skset, nullptr, 1000);
        }

        if (enet_host_ != nullptr) {
            HandleService();
        }

        if (ret > 0) {
            if (ENET_SOCKETSET_CHECK(skset, pipe_rh)) {
                HandleAsyncCommand();
            }
            continue;
        }

        if (ret < 0) {
            ASBLog(ERROR) << "ENet select failed, " << strerror(errno);
            if (errno == EINTR) {
                continue;
            }
            break;
        }
    }
}

void Context::HandleService() {
    assert(enet_host_ != nullptr);

    ENetEvent enet_evt;

    while (enet_host_service(enet_host_, &enet_evt, 0) > 0) {
        switch (enet_evt.type) {
        case ENET_EVENT_TYPE_RECEIVE:
            HandlePacket(enet_evt.peer, enet_evt.packet);
            break;
        case ENET_EVENT_TYPE_CONNECT:
            HandleConnect(enet_evt.peer);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
        case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
            HandleDisconnect(enet_evt.peer);
            break;
        default:
            ASBLog(ERROR) << "Unknown event: " << enet_evt.type;
            break;
        }
    }
}

void Context::HandleConnect(ENetPeer* enet_peer) {
    ASBLog(INFO) << "Peer " << enet_peer << " connected.";
    auto it = pending_connections_.find(enet_peer);
    if (it != pending_connections_.end()) {
        ASBLog(INFO) << "Pending connection callback for peer " << enet_peer;
        auto cb = std::move(it->second);
        pending_connections_.erase(it);
        ScopedUnlocker<std::mutex> unlocker(mutex_);
        cb(Result::kOk, enet_peer);
        return;
    }
    auto cb = connect_handler_;
    ScopedUnlocker<std::mutex> unlocker(mutex_);
    enet_peer_timeout(enet_peer, 3, 1000, 4000);
    cb(enet_peer);
}

void Context::HandleDisconnect(ENetPeer* enet_peer) {
    //ASBLog(INFO) << "Peer " << enet_peer << " disconnected.";
    auto pdit = pending_disconnections_.find(enet_peer);
    if (pdit != pending_disconnections_.end()) {
        auto cb = std::move(pdit->second);
        pending_disconnections_.erase(pdit);
        ScopedUnlocker<std::mutex> unlocker(mutex_);
        cb(Result::kOk);
        return;
    }
    auto pcit = pending_connections_.find(enet_peer);
    if (pcit != pending_connections_.end()) {
        auto cb = std::move(pcit->second);
        pending_connections_.erase(pcit);
        ScopedUnlocker<std::mutex> unlocker(mutex_);
        cb(Result::kTimeout, nullptr);
        return;
    }
    auto dhit = disconnect_handler_map_.find(enet_peer);
    if (dhit != disconnect_handler_map_.end()) {
        auto cb = std::move(dhit->second);
        disconnect_handler_map_.erase(dhit);
        ScopedUnlocker<std::mutex> unlocker(mutex_);
        cb();
        return;
    }
    //ASBLog(INFO) << "Unknown peer " << enet_peer << " disconnection.";
}

void Context::HandlePacket(ENetPeer* enet_peer, ENetPacket* enet_pkt) {
    //ASBLog(INFO) << "Receive packet from peer " << enet_peer;
    auto enet_pkt_guard = MakeScopeGuard([enet_pkt]{ enet_packet_destroy(enet_pkt); });
    auto size = enet_packet_get_length(enet_pkt);
    if (size < (sizeof(protocol::Header))) {
        ASBLog(ERROR) << "Receive packet from enet peer " << enet_peer << " is too small.";
        return;
    }
    uint8_t* data = reinterpret_cast<uint8_t*>(enet_packet_get_data(enet_pkt));
    auto header = reinterpret_cast<protocol::Header*>(data);
    if (header->version < protocol::kVersion) {
        ASBLog(ERROR) << "Protocol version is mismatch, received is " << header->version << ", expected " << protocol::kVersion;
        return;
    }
    if (header->type >= static_cast<uint8_t>(protocol::Type::kMax)) {
        ASBLog(ERROR) << "Receive packet with invalid type: " << header->type;
        return;
    }
    if (header->opcode >= static_cast<uint8_t>(protocol::Opcode::kMax)) {
        ASBLog(ERROR) << "Receive packet with invalid opcode: " << header->opcode;
        return;
    }
    data += sizeof(protocol::Header);
    size -= sizeof(protocol::Header);
    google::protobuf::io::ArrayInputStream payload(data, size);
    switch (static_cast<protocol::Type>(header->type)) {
    case protocol::Type::kEvent:
        HandleEventPacket(enet_peer, static_cast<protocol::Opcode>(header->opcode), payload);
        break;
    case protocol::Type::kRequest:
        HandleRequestPacket(enet_peer, static_cast<protocol::Opcode>(header->opcode), ntohl(header->session), payload);
        break;
    case protocol::Type::kResponse:
        HandleResponsePacket(enet_peer, ntohl(header->session), static_cast<Result>(ntohl(header->extra_data)), payload);
        break;
    default:
        ASBLog(ERROR) << "Unknown packet type " << header->type;
        break;
    }
}

void Context::HandleEventPacket(ENetPeer* enet_peer, protocol::Opcode opcode, google::protobuf::io::ZeroCopyInputStream& payload) {
    //ASBLog(INFO) << "Receive event packet from peer " << enet_peer;
    auto pit = event_handler_map_.find(enet_peer);
    if (pit != event_handler_map_.end()) {
        auto hit = pit->second.find(opcode);
        if (hit != pit->second.end()) {
            auto cb = hit->second;
            ScopedUnlocker<std::mutex> unlocker(mutex_);
            cb(payload);
        } else {
            ASBLog(INFO) << "No event " << static_cast<uint8_t>(opcode) << " handler";
        }
    } else {
        ASBLog(INFO) << "No event " << static_cast<uint8_t>(opcode) << " handler";
    }
}

void Context::HandleRequestPacket(ENetPeer* enet_peer, protocol::Opcode opcode, uint32_t session, google::protobuf::io::ZeroCopyInputStream& payload) {
    //ASBLog(INFO) << "Receive request packet from peer " << enet_peer << ", sessoin " << session;
    auto pit = request_handler_map_.find(enet_peer);
    if (pit != request_handler_map_.end()) {
        auto hit = pit->second.find(opcode);
        if (hit != pit->second.end()) {
            RequestContext request_context(shared_from_this(), enet_peer, opcode, session, payload);
            auto cb = hit->second;
            ScopedUnlocker<std::mutex> unlocker(mutex_);
            cb(request_context);
        } else {
            ASBLog(INFO) << "No request handler " << static_cast<uint8_t>(opcode) << " handler";
        }
    } else {
        ASBLog(INFO) << "No request handler " << static_cast<uint8_t>(opcode) << " handler";
    }
}

void Context::HandleResponsePacket(ENetPeer* enet_peer, uint32_t session, Result result, google::protobuf::io::ZeroCopyInputStream& payload) {
    //ASBLog(INFO) << "Receive response packet from peer " << enet_peer;
    auto pit = request_session_map_.find(enet_peer);
    if (pit != request_session_map_.end()) {
        auto hit = pit->second.find(session);
        if (hit != pit->second.end()) {
            auto cb = std::move(hit->second);
            pit->second.erase(hit);
            if (pit->second.empty()) {
                request_session_map_.erase(pit);
            }
            ScopedUnlocker<std::mutex> unlocker(mutex_);
            cb(result, &payload);
        } else {
            ASBLog(INFO) << "No request session " << session;
        }
    } else {
        ASBLog(INFO) << "No request session " << session;
    }
}

void Context::HandleAsyncCommand() {
    assert(pipe_.IsOpen());
    char async_cmd;
    if (pipe_.Read(&async_cmd, 1) == 1) {
        switch (async_cmd) {
        case AsyncCommand::kAsyncWakeup:
            // Nothing to do.
            break;
        case AsyncCommand::kAsyncExit:
            ASBLog(INFO) << "Handle async exit.";
            backend_run_ = false;
            break;
        default:
            ASBLog(ERROR) << "Unknown async command " << async_cmd;
            break;
        }
    }
}

bool Context::SendPacket(ENetPeer* enet_peer, ENetPacket* enet_pkt) {
    if (enet_peer == nullptr || enet_pkt == nullptr) {
        return false;
    }
    if (enet_peer_send(enet_peer, 0, enet_pkt) < 0) {
        return false;
    }
    WakeupBackend();
    return true;
}

bool Context::SendPacket(ENetPeer* enet_peer, protocol::Type type, protocol::Opcode opcode, uint32_t session, const google::protobuf::Message* payload) {
    if (enet_peer == nullptr) {
        ASBLog(ERROR) << "Invalid enet peer to send.";
        return false;
    }
    if (enet_host_ == nullptr || opcode > protocol::Opcode::kMax) {
        ASBLog(ERROR) << "Send packet with invalid enet host or invalid packet.";
        return false;
    }
    auto enet_pkt = CreatePacket(type, opcode, session, payload);
    if (enet_pkt == nullptr) {
        ASBLog(ERROR) << "Failed to create packet to send.";
        return false;
    }
    if (enet_peer_send(enet_peer, 0, enet_pkt) < 0) {
        enet_packet_destroy(enet_pkt);
        ASBLog(ERROR) << "Failed to send packet for peer " << enet_peer;
        return false;
    }
    WakeupBackend();
    return true;
}

ENetPacket* Context::CreateResponsePacket(protocol::Opcode opcode, uint32_t session, Result result, const google::protobuf::Message* payload) {
    return CreatePacket(protocol::Type::kResponse, opcode, session, payload, static_cast<uint32_t>(result));
}

ENetPacket* Context::CreatePacket(protocol::Type type, protocol::Opcode opcode, uint32_t session,
                                  const google::protobuf::Message* payload, uint32_t extra_data) {
    auto size = sizeof(protocol::Header);
    if (payload != nullptr) {
        size += payload->ByteSizeLong();
    }
    auto enet_pkt = enet_packet_create(nullptr, size, ENET_PACKET_FLAG_RELIABLE);
    if (enet_pkt == nullptr) {
        ASBLog(ERROR) << "Failed to create enet packet.";
        return nullptr;
    }
    auto enet_pkt_guard = MakeScopeGuard([enet_pkt] { enet_packet_destroy(enet_pkt); });
    uint8_t* data = reinterpret_cast<uint8_t*>(enet_packet_get_data(enet_pkt));
    auto header = reinterpret_cast<protocol::Header*>(data);
    header->version = protocol::kVersion;
    header->type = static_cast<uint8_t>(type);
    header->opcode = static_cast<uint8_t>(opcode);
    header->session = htonl(session);
    header->extra_data = htonl(extra_data);
    data += sizeof(protocol::Header);
    size -= sizeof(protocol::Header);
    if (payload != nullptr && !payload->SerializeToArray(data, size)) {
        ASBLog(ERROR) << "Failed to serialize payload.";
        return nullptr;
    }
    enet_pkt_guard.Dismiss();
    return enet_pkt;
}

}
}
}
