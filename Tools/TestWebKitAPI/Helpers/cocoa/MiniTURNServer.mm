/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Helpers/cocoa/MiniTURNServer.h"

#import "Helpers/Utilities.h"
#import <CommonCrypto/CommonDigest.h>
#import <CommonCrypto/CommonHMAC.h>
#import <atomic>
#import <netinet/in.h>
#import <optional>
#import <string>
#import <wtf/BlockPtr.h>
#import <wtf/Function.h>
#import <wtf/Lock.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

namespace TestWebKitAPI {

// STUN/TURN wire constants (RFC 5389 / RFC 5766).
static constexpr uint32_t kStunMagicCookie = 0x2112A442;
static constexpr uint16_t kStunHeaderSize = 20;

static constexpr uint16_t kStunMethodBinding = 0x001;
static constexpr uint16_t kStunMethodAllocate = 0x003;
static constexpr uint16_t kStunMethodRefresh = 0x004;
static constexpr uint16_t kStunMethodCreatePermission = 0x008;

static constexpr uint16_t kStunAttrMessageIntegrity = 0x0008;
static constexpr uint16_t kStunAttrErrorCode = 0x0009;
static constexpr uint16_t kStunAttrLifetime = 0x000D;
static constexpr uint16_t kStunAttrRealm = 0x0014;
static constexpr uint16_t kStunAttrNonce = 0x0015;
static constexpr uint16_t kStunAttrXorRelayedAddress = 0x0016;
static constexpr uint16_t kStunAttrXorMappedAddress = 0x0020;

// Long-term credentials shared with the JS side of the test.
static constexpr auto kMockTurnUsername = "testUser";
static constexpr auto kMockTurnPassword = "testPass";
static constexpr auto kMockTurnRealm = "webkit.test";
static constexpr auto kMockTurnNonce = "webkit-mock-turn-nonce-12345678";

class MiniTURNServerState : public ThreadSafeRefCounted<MiniTURNServerState> {
public:
    static Ref<MiniTURNServerState> create() { return adoptRef(*new MiniTURNServerState); }

    void recordRequest(MiniTURNServer::Transport transport, uint16_t method)
    {
        Locker locker { m_lock };
        m_requests.append({ transport, method });
    }

    Vector<MiniTURNServer::Request> takeRequests()
    {
        Locker locker { m_lock };
        return std::exchange(m_requests, { });
    }

    void setUDPPort(uint16_t port) { m_udpPort.store(port); }
    void setTCPPort(uint16_t port) { m_tcpPort.store(port); }
    uint16_t udpPort() const { return m_udpPort.load(); }
    uint16_t tcpPort() const { return m_tcpPort.load(); }

    RetainPtr<nw_listener_t> startListener(MiniTURNServer::Transport, Function<void(uint16_t)>&&);

private:
    MiniTURNServerState() = default;

    void handleStunMessage(std::span<const uint8_t>, MiniTURNServer::Transport, const sockaddr* peer, NOESCAPE const Function<void(std::span<const uint8_t>)>& reply);
    void receiveUDPDatagram(RetainPtr<nw_connection_t>&&);
    void receiveTCPBytes(RetainPtr<nw_connection_t>&&, Vector<uint8_t>&& accumulated = { });

    Lock m_lock;
    Vector<MiniTURNServer::Request> m_requests WTF_GUARDED_BY_LOCK(m_lock);
    std::atomic<uint16_t> m_udpPort { 0 };
    std::atomic<uint16_t> m_tcpPort { 0 };
};

static uint16_t extractStunMethod(uint16_t messageType)
{
    return (messageType & 0x000F) | ((messageType & 0x00E0) >> 1) | ((messageType & 0x3E00) >> 2);
}

static uint16_t makeStunSuccessMessageType(uint16_t method)
{
    // Success response class bits are C1=1, C0=0 → 0x0100.
    uint16_t type = 0x0100;
    type |= (method & 0x000F);
    type |= (method & 0x0070) << 1;
    type |= (method & 0x0F80) << 2;
    return type;
}

static uint16_t makeStunErrorMessageType(uint16_t method)
{
    // Error response class bits are C1=1, C0=1 → 0x0110.
    uint16_t type = 0x0110;
    type |= (method & 0x000F);
    type |= (method & 0x0070) << 1;
    type |= (method & 0x0F80) << 2;
    return type;
}

static void writeU16BE(Vector<uint8_t>& out, uint16_t v)
{
    out.append(static_cast<uint8_t>(v >> 8));
    out.append(static_cast<uint8_t>(v & 0xff));
}

static void writeU32BE(Vector<uint8_t>& out, uint32_t v)
{
    writeU16BE(out, static_cast<uint16_t>(v >> 16));
    writeU16BE(out, static_cast<uint16_t>(v & 0xffff));
}

static void appendXorAddressAttributeV4(Vector<uint8_t>& out, uint16_t attrType, uint32_t ipv4HostOrder, uint16_t port)
{
    writeU16BE(out, attrType);
    writeU16BE(out, 8);
    out.append(0x00);
    out.append(0x01);
    writeU16BE(out, port ^ static_cast<uint16_t>(kStunMagicCookie >> 16));
    writeU32BE(out, ipv4HostOrder ^ kStunMagicCookie);
}

static void appendXorAddressAttributeV6(Vector<uint8_t>& out, uint16_t attrType, std::span<const uint8_t, 16> ipv6, uint16_t port, std::span<const uint8_t> txnId)
{
    writeU16BE(out, attrType);
    writeU16BE(out, 20);
    out.append(0x00);
    out.append(0x02);
    writeU16BE(out, port ^ static_cast<uint16_t>(kStunMagicCookie >> 16));
    uint8_t xorKey[16] = {
        0x21, 0x12, 0xA4, 0x42,
        txnId[0], txnId[1], txnId[2], txnId[3],
        txnId[4], txnId[5], txnId[6], txnId[7],
        txnId[8], txnId[9], txnId[10], txnId[11],
    };
    uint8_t xored[16];
    for (size_t i = 0; i < 16; ++i)
        xored[i] = ipv6[i] ^ xorKey[i];
    out.append(std::span<const uint8_t> { xored, 16 });
}

static void appendLifetimeAttribute(Vector<uint8_t>& out, uint32_t seconds)
{
    writeU16BE(out, kStunAttrLifetime);
    writeU16BE(out, 4);
    writeU32BE(out, seconds);
}

static void appendPadTo4(Vector<uint8_t>& out)
{
    while (out.size() % 4)
        out.append(0);
}

static void appendStringAttribute(Vector<uint8_t>& out, uint16_t attrType, const char* value)
{
    auto length = strlen(value);
    writeU16BE(out, attrType);
    writeU16BE(out, static_cast<uint16_t>(length));
    out.append(std::span<const uint8_t> { reinterpret_cast<const uint8_t*>(value), length });
    appendPadTo4(out);
}

static void appendErrorCodeAttribute(Vector<uint8_t>& out, uint16_t errorCode, const char* reason)
{
    auto reasonLength = strlen(reason);
    writeU16BE(out, kStunAttrErrorCode);
    writeU16BE(out, static_cast<uint16_t>(4 + reasonLength));
    out.append(0x00);
    out.append(0x00);
    out.append(static_cast<uint8_t>(errorCode / 100));
    out.append(static_cast<uint8_t>(errorCode % 100));
    out.append(std::span<const uint8_t> { reinterpret_cast<const uint8_t*>(reason), reasonLength });
    appendPadTo4(out);
}

// Long-term credential key = MD5(username ":" realm ":" password) per RFC 5389 §15.4.
static Vector<uint8_t> computeLongTermAuthKey(const char* username, const char* realm, const char* password)
{
    std::string input;
    input.reserve(strlen(username) + strlen(realm) + strlen(password) + 2);
    input.append(username);
    input.push_back(':');
    input.append(realm);
    input.push_back(':');
    input.append(password);
    Vector<uint8_t> key(CC_MD5_DIGEST_LENGTH);
    CC_MD5(input.data(), static_cast<CC_LONG>(input.size()), key.mutableSpan().data());
    return key;
}

// Appends MESSAGE-INTEGRITY to `message` per RFC 5389 §15.4.
static void appendMessageIntegrityAttribute(Vector<uint8_t>& message, std::span<const uint8_t> key)
{
    constexpr uint16_t messageIntegrityAttrSize = 24; // 4-byte header + 20-byte HMAC.
    uint16_t finalBodyLength = static_cast<uint16_t>(message.size() - kStunHeaderSize + messageIntegrityAttrSize);
    message[2] = static_cast<uint8_t>(finalBodyLength >> 8);
    message[3] = static_cast<uint8_t>(finalBodyLength & 0xff);

    uint8_t hmac[CC_SHA1_DIGEST_LENGTH];
    CCHmac(kCCHmacAlgSHA1, key.data(), key.size(), message.span().data(), message.size(), hmac);

    writeU16BE(message, kStunAttrMessageIntegrity);
    writeU16BE(message, static_cast<uint16_t>(CC_SHA1_DIGEST_LENGTH));
    message.append(std::span<const uint8_t> { hmac, CC_SHA1_DIGEST_LENGTH });
}

static bool hasMessageIntegrityAttribute(std::span<const uint8_t> message)
{
    if (message.size() < kStunHeaderSize)
        return false;
    size_t pos = kStunHeaderSize;
    while (pos + 4 <= message.size()) {
        uint16_t attrType = (static_cast<uint16_t>(message[pos]) << 8) | message[pos + 1];
        uint16_t attrLen = (static_cast<uint16_t>(message[pos + 2]) << 8) | message[pos + 3];
        pos += 4;
        if (attrType == kStunAttrMessageIntegrity)
            return true;
        pos += (attrLen + 3) & ~static_cast<size_t>(3);
    }
    return false;
}

static RetainPtr<nw_endpoint_t> peerEndPointAddress(nw_connection_t conn)
{
    RetainPtr endpoint = adoptNS(nw_connection_copy_endpoint(conn));
    if (nw_endpoint_get_type(endpoint.get()) != nw_endpoint_type_address)
        return { };
    return endpoint;
}

void MiniTURNServerState::handleStunMessage(std::span<const uint8_t> message, MiniTURNServer::Transport transport, const sockaddr* peer, NOESCAPE const Function<void(std::span<const uint8_t>)>& reply)
{
    if (!peer) {
        NSLog(@"MiniTURNServer handleStunMessage: no sockaddr available");
        return;
    }

    if (message.size() < kStunHeaderSize)
        return;

    uint16_t messageType = (static_cast<uint16_t>(message[0]) << 8) | message[1];
    uint16_t method = extractStunMethod(messageType);
    uint32_t cookie = (static_cast<uint32_t>(message[4]) << 24)
        | (static_cast<uint32_t>(message[5]) << 16)
        | (static_cast<uint32_t>(message[6]) << 8)
        | message[7];
    if (cookie != kStunMagicCookie)
        return;

    recordRequest(transport, method);

    if (method != kStunMethodBinding && method != kStunMethodAllocate
        && method != kStunMethodRefresh && method != kStunMethodCreatePermission)
        return;

    auto txnId = message.subspan(8, 12);
    bool authenticated = hasMessageIntegrityAttribute(message);

    // If the request lacks MESSAGE-INTEGRITY, the client is doing its first request and we reply with a 401 challenge.
    if (method != kStunMethodBinding && !authenticated) {
        Vector<uint8_t> body;
        appendErrorCodeAttribute(body, 401, "Unauthorized");
        appendStringAttribute(body, kStunAttrRealm, kMockTurnRealm);
        appendStringAttribute(body, kStunAttrNonce, kMockTurnNonce);

        Vector<uint8_t> out;
        writeU16BE(out, makeStunErrorMessageType(method));
        writeU16BE(out, static_cast<uint16_t>(body.size()));
        writeU32BE(out, kStunMagicCookie);
        out.append(txnId);
        out.append(body.span());
        reply(out.span());
        return;
    }

    // Extract peer address + port and decide which family to encode.
    bool peerIsV4 = false;
    uint16_t clientPort = 0;
    uint32_t clientV4HostOrder = 0;
    const uint8_t* clientV6Bytes = nullptr;
    if (peer->sa_family == AF_INET) {
        peerIsV4 = true;
        auto* v4 = reinterpret_cast<const sockaddr_in*>(peer);
        clientPort = ntohs(v4->sin_port);
        clientV4HostOrder = ntohl(v4->sin_addr.s_addr);
    } else if (peer->sa_family == AF_INET6) {
        auto* v6 = reinterpret_cast<const sockaddr_in6*>(peer);
        clientPort = ntohs(v6->sin6_port);
        if (IN6_IS_ADDR_V4MAPPED(&v6->sin6_addr)) {
            peerIsV4 = true;
            clientV4HostOrder = ntohl(*reinterpret_cast<const uint32_t*>(&v6->sin6_addr.s6_addr[12]));
        } else
            clientV6Bytes = v6->sin6_addr.s6_addr;
    } else
        return;

    // Emit XOR-MAPPED / XOR-RELAYED in the family matching the peer so libwebrtc's TurnPort accepts them alongside its own network family.
    auto appendPeerXorMapped = [&](Vector<uint8_t>& out) {
        if (peerIsV4)
            appendXorAddressAttributeV4(out, kStunAttrXorMappedAddress, clientV4HostOrder, clientPort);
        else
            appendXorAddressAttributeV6(out, kStunAttrXorMappedAddress, std::span<const uint8_t, 16> { clientV6Bytes, 16 }, clientPort, txnId);
    };
    auto appendLoopbackXorRelayed = [&](Vector<uint8_t>& out) {
        uint16_t basePort = transport == MiniTURNServer::Transport::Udp ? udpPort() : tcpPort();
        uint16_t relayPort = basePort + 1;
        if (peerIsV4)
            appendXorAddressAttributeV4(out, kStunAttrXorRelayedAddress, INADDR_LOOPBACK, relayPort);
        else {
            static constexpr uint8_t v6Loopback[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
            appendXorAddressAttributeV6(out, kStunAttrXorRelayedAddress, std::span<const uint8_t, 16> { v6Loopback, 16 }, relayPort, txnId);
        }
    };

    Vector<uint8_t> body;
    if (method == kStunMethodAllocate) {
        appendLoopbackXorRelayed(body);
        appendPeerXorMapped(body);
        appendLifetimeAttribute(body, 600);
    } else if (method == kStunMethodRefresh)
        appendLifetimeAttribute(body, 600);
    else if (method == kStunMethodBinding)
        appendPeerXorMapped(body);

    Vector<uint8_t> out;
    writeU16BE(out, makeStunSuccessMessageType(method));
    writeU16BE(out, static_cast<uint16_t>(body.size()));
    writeU32BE(out, kStunMagicCookie);
    out.append(txnId);
    out.append(body.span());

    if (authenticated) {
        auto key = computeLongTermAuthKey(kMockTurnUsername, kMockTurnRealm, kMockTurnPassword);
        appendMessageIntegrityAttribute(out, key.span());
    }

    reply(out.span());
}

static void sendReply(nw_connection_t connection, std::span<const uint8_t> reply, bool isComplete)
{
    OSObjectPtr data = adoptOSObject(dispatch_data_create(reply.data(), reply.size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT));
    nw_connection_send(connection, data.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, isComplete, ^(nw_error_t sendError) {
        if (sendError)
            NSLog(@"MiniTURNServer send error %d", nw_error_get_error_code(sendError));
    });
}

void MiniTURNServerState::receiveUDPDatagram(RetainPtr<nw_connection_t>&& connection)
{
    auto* rawConnection = connection.get();
    nw_connection_receive_message(rawConnection, makeBlockPtr([protectedThis = Ref { *this }, connection = WTF::move(connection)](dispatch_data_t content, nw_content_context_t, bool, nw_error_t error) mutable {
        if (error) {
            NSLog(@"MiniTURNServer UDP receive error %d", nw_error_get_error_code(error));
            return;
        }
        if (content && dispatch_data_get_size(content) >= kStunHeaderSize) {
            Vector<uint8_t> bytes;
            dispatch_data_apply_span(content, [&](std::span<const uint8_t> chunk) {
                bytes.append(chunk);
                return true;
            });

            RetainPtr endpoint = peerEndPointAddress(connection.get());
            protectedThis->handleStunMessage(bytes.span(), MiniTURNServer::Transport::Udp, nw_endpoint_get_address(endpoint.get()), [&connection](auto reply) {
                bool isComplete = true;
                sendReply(connection.get(), reply, isComplete);
            });
        }
        protectedThis->receiveUDPDatagram(WTF::move(connection));
    }).get());
}

void MiniTURNServerState::receiveTCPBytes(RetainPtr<nw_connection_t>&& connection, Vector<uint8_t>&& accumulated)
{
    auto* rawConnection = connection.get();
    nw_connection_receive(rawConnection, 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([protectedThis = Ref { *this }, connection = WTF::move(connection), accumulated = WTF::move(accumulated)](dispatch_data_t content, nw_content_context_t, bool isComplete, nw_error_t error) mutable {
        if (error) {
            NSLog(@"MiniTURNServer TCP receive error %d", nw_error_get_error_code(error));
            return;
        }
        if (content) {
            dispatch_data_apply_span(content, [&](std::span<const uint8_t> chunk) {
                accumulated.append(chunk);
                return true;
            });
        }

        RetainPtr endpoint = peerEndPointAddress(connection.get());
        while (accumulated.size() >= kStunHeaderSize) {
            uint16_t bodyLen = (static_cast<uint16_t>(accumulated[2]) << 8) | accumulated[3];
            size_t total = kStunHeaderSize + bodyLen;
            if (accumulated.size() < total)
                break;
            std::span<const uint8_t> message { accumulated.span().data(), total };
            protectedThis->handleStunMessage(message, MiniTURNServer::Transport::Tcp, nw_endpoint_get_address(endpoint.get()), [&connection](auto reply) {
                bool isComplete = false;
                sendReply(connection.get(), reply, isComplete);
            });
            accumulated.removeAt(0, total);
        }
        if (!isComplete)
            protectedThis->receiveTCPBytes(WTF::move(connection), WTF::move(accumulated));
    }).get());
}

RetainPtr<nw_listener_t> MiniTURNServerState::startListener(MiniTURNServer::Transport transport, Function<void(uint16_t)>&& onReady)
{
    bool isUDP = transport == MiniTURNServer::Transport::Udp;
    RetainPtr<nw_parameters_t> parameters = isUDP
        ? adoptNS(nw_parameters_create_secure_udp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION))
        : adoptNS(nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION));

    RetainPtr listener = adoptNS(nw_listener_create(parameters.get()));
    nw_listener_set_queue(listener.get(), mainDispatchQueueSingleton());

    nw_listener_set_state_changed_handler(listener.get(), makeBlockPtr([listener, onReady = WTF::move(onReady), isUDP](nw_listener_state_t state, nw_error_t error) mutable {
        if (state == nw_listener_state_failed) {
            NSLog(@"MiniTURNServer %s listener failed with error %d", isUDP ? "UDP" : "TCP", error ? nw_error_get_error_code(error) : 0);
            return;
        }
        if (state == nw_listener_state_ready && onReady) {
            uint16_t port = nw_listener_get_port(listener.get());
            auto handler = std::exchange(onReady, { });
            handler(port);
        }
    }).get());

    nw_listener_set_new_connection_handler(listener.get(), makeBlockPtr([protectedThis = Ref { *this }, isUDP](nw_connection_t connection) {
        nw_connection_set_queue(connection, mainDispatchQueueSingleton());
        nw_connection_start(connection);
        if (isUDP)
            protectedThis->receiveUDPDatagram({ connection });
        else
            protectedThis->receiveTCPBytes({ connection });
    }).get());

    nw_listener_start(listener.get());
    return listener;
}

MiniTURNServer::MiniTURNServer()
    : m_state(MiniTURNServerState::create())
{
    bool udpReady = false;
    bool tcpReady = false;
    m_udpListener = m_state->startListener(Transport::Udp, [&udpReady, state = Ref { m_state }](uint16_t port) {
        state->setUDPPort(port);
        udpReady = true;
    });
    m_tcpListener = m_state->startListener(Transport::Tcp, [&tcpReady, state = Ref { m_state }](uint16_t port) {
        state->setTCPPort(port);
        tcpReady = true;
    });
    Util::run(&udpReady);
    Util::run(&tcpReady);
}

MiniTURNServer::~MiniTURNServer()
{
    if (auto listener = std::exchange(m_udpListener, { }))
        nw_listener_cancel(listener.get());
    if (auto listener = std::exchange(m_tcpListener, { }))
        nw_listener_cancel(listener.get());
}

uint16_t MiniTURNServer::udpPort() const
{
    return m_state->udpPort();
}

uint16_t MiniTURNServer::tcpPort() const
{
    return m_state->tcpPort();
}

auto MiniTURNServer::takeRequests() -> Vector<Request>
{
    return m_state->takeRequests();
}

bool MiniTURNServer::isStunMethodAllocate(uint16_t method)
{
    return method == kStunMethodAllocate;
}

} // namespace TestWebKitAPI
