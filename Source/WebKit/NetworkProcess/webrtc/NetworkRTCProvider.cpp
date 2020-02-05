/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkRTCProvider.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCSocketClient.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkRTCResolver.h"
#include "NetworkRTCSocket.h"
#include "WebRTCResolverMessages.h"
#include "WebRTCSocketMessages.h"
#include <WebCore/LibWebRTCMacros.h>
#include <webrtc/rtc_base/async_packet_socket.h>
#include <webrtc/rtc_base/logging.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "NetworkRTCResolverCocoa.h"
#endif

namespace WebKit {
using namespace WebCore;

#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(canLog(), Network, "%p - NetworkRTCProvider::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(canLog(), Network, "%p - NetworkRTCProvider::" fmt, this, ##__VA_ARGS__)

static inline std::unique_ptr<rtc::Thread> createThread()
{
    auto thread = rtc::Thread::CreateWithSocketServer();
    auto result = thread->Start();
    ASSERT_UNUSED(result, result);
    // FIXME: Set thread name.
    return thread;
}

#if !RELEASE_LOG_DISABLED
static void doReleaseLogging(rtc::LoggingSeverity severity, const char* message)
{
    if (severity == rtc::LS_ERROR)
        RELEASE_LOG_ERROR(WebRTC, "LibWebRTC error: %{public}s", message);
    else
        RELEASE_LOG(WebRTC, "LibWebRTC message: %{public}s", message);
}
#endif

NetworkRTCProvider::NetworkRTCProvider(NetworkConnectionToWebProcess& connection)
    : m_connection(&connection)
    , m_rtcMonitor(*this)
    , m_rtcNetworkThread(createThread())
    , m_packetSocketFactory(makeUniqueRefWithoutFastMallocCheck<rtc::BasicPacketSocketFactory>(m_rtcNetworkThread.get()))
    , m_canLog(connection.sessionID().isAlwaysOnLoggingAllowed())
{
#if !RELEASE_LOG_DISABLED
    rtc::LogMessage::SetLogOutput(WebKit2LogWebRTC.state == WTFLogChannelState::On ? rtc::LS_INFO : rtc::LS_WARNING, doReleaseLogging);
#endif
}

NetworkRTCProvider::~NetworkRTCProvider()
{
    ASSERT(!m_connection);
    ASSERT(!m_sockets.size());
    ASSERT(!m_rtcMonitor.isStarted());
}

void NetworkRTCProvider::close()
{
    RELEASE_LOG_IF_ALLOWED("close");

    // Cancel all pending DNS resolutions.
    while (!m_resolvers.isEmpty())
        stopResolver(*m_resolvers.keys().begin());

    m_connection = nullptr;
    m_rtcMonitor.stopUpdating();

    callOnRTCNetworkThread([this]() {
        m_sockets.clear();
        callOnMainThread([provider = makeRef(*this)]() {
            if (provider->m_rtcNetworkThread)
                provider->m_rtcNetworkThread->Stop();
        });
    });
}

void NetworkRTCProvider::createSocket(LibWebRTCSocketIdentifier identifier, std::unique_ptr<rtc::AsyncPacketSocket>&& socket, Socket::Type type)
{
    if (!socket) {
        sendFromMainThread([this, identifier, size = m_sockets.size()](IPC::Connection& connection) {
            RELEASE_LOG_ERROR_IF_ALLOWED("createSocket with %u sockets is unable to create a new socket", size);
            connection.send(Messages::WebRTCSocket::SignalClose(1), identifier);
        });
        return;
    }
    addSocket(identifier, makeUnique<LibWebRTCSocketClient>(identifier, *this, WTFMove(socket), type));
}

void NetworkRTCProvider::createUDPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& address, uint16_t minPort, uint16_t maxPort)
{
    callOnRTCNetworkThread([this, identifier, address = RTCNetwork::isolatedCopy(address.value), minPort, maxPort]() {
        std::unique_ptr<rtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateUdpSocket(address, minPort, maxPort));
        createSocket(identifier, WTFMove(socket), Socket::Type::UDP);
    });
}

void NetworkRTCProvider::createServerTCPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& address, uint16_t minPort, uint16_t maxPort, int options)
{
    if (!m_isListeningSocketAuthorized) {
        if (m_connection)
            m_connection->connection().send(Messages::WebRTCSocket::SignalClose(1), identifier);
        return;
    }

    callOnRTCNetworkThread([this, identifier, address = RTCNetwork::isolatedCopy(address.value), minPort, maxPort, options]() {
        std::unique_ptr<rtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateServerTcpSocket(address, minPort, maxPort, options));
        createSocket(identifier, WTFMove(socket), Socket::Type::ServerTCP);
    });
}

#if !PLATFORM(COCOA)
rtc::ProxyInfo NetworkRTCProvider::proxyInfoFromSession(const RTCNetwork::SocketAddress&, NetworkSession&)
{
    return { };
}
#endif

void NetworkRTCProvider::createClientTCPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& localAddress, const RTCNetwork::SocketAddress& remoteAddress, String&& userAgent, int options)
{
    auto* session = m_connection->networkSession();
    if (!session) {
        m_connection->connection().send(Messages::WebRTCSocket::SignalClose(1), identifier);
        return;
    }
    callOnRTCNetworkThread([this, identifier, localAddress = RTCNetwork::isolatedCopy(localAddress.value), remoteAddress = RTCNetwork::isolatedCopy(remoteAddress.value), proxyInfo = proxyInfoFromSession(remoteAddress, *session), userAgent = WTFMove(userAgent).isolatedCopy(), options]() {
        rtc::PacketSocketTcpOptions tcpOptions;
        tcpOptions.opts = options;
        std::unique_ptr<rtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateClientTcpSocket(localAddress, remoteAddress, proxyInfo, userAgent.utf8().data(), tcpOptions));
        createSocket(identifier, WTFMove(socket), Socket::Type::ClientTCP);
    });
}

void NetworkRTCProvider::wrapNewTCPConnection(LibWebRTCSocketIdentifier identifier, LibWebRTCSocketIdentifier newConnectionSocketIdentifier)
{
    callOnRTCNetworkThread([this, identifier, newConnectionSocketIdentifier]() {
        std::unique_ptr<rtc::AsyncPacketSocket> socket = m_pendingIncomingSockets.take(newConnectionSocketIdentifier);
        addSocket(identifier, makeUnique<LibWebRTCSocketClient>(identifier, *this, WTFMove(socket), Socket::Type::ServerConnectionTCP));
    });
}

void NetworkRTCProvider::addSocket(LibWebRTCSocketIdentifier identifier, std::unique_ptr<Socket>&& socket)
{
    m_sockets.add(identifier, WTFMove(socket));
}

std::unique_ptr<NetworkRTCProvider::Socket> NetworkRTCProvider::takeSocket(LibWebRTCSocketIdentifier identifier)
{
    return m_sockets.take(identifier);
}

void NetworkRTCProvider::newConnection(Socket& serverSocket, std::unique_ptr<rtc::AsyncPacketSocket>&& newSocket)
{
    auto incomingSocketIdentifier = LibWebRTCSocketIdentifier::generate();
    sendFromMainThread([identifier = serverSocket.identifier(), incomingSocketIdentifier, remoteAddress = RTCNetwork::isolatedCopy(newSocket->GetRemoteAddress())](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalNewConnection(incomingSocketIdentifier, RTCNetwork::SocketAddress(remoteAddress)), identifier);
    });
    m_pendingIncomingSockets.add(incomingSocketIdentifier, WTFMove(newSocket));
}

void NetworkRTCProvider::didReceiveNetworkRTCSocketMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    NetworkRTCSocket(makeObjectIdentifier<LibWebRTCSocketIdentifierType>(decoder.destinationID()), *this).didReceiveMessage(connection, decoder);
}

#if PLATFORM(COCOA)

void NetworkRTCProvider::createResolver(uint64_t identifier, const String& address)
{
    ASSERT(m_resolvers.isValidKey(identifier));
    ASSERT(!address.isEmpty());
    if (!m_resolvers.isValidKey(identifier) || address.isEmpty())
        return;

    auto resolver = NetworkRTCResolver::create(identifier, [this, identifier](WebCore::DNSAddressesOrError&& result) mutable {
        if (!result.has_value()) {
            if (result.error() != WebCore::DNSError::Cancelled)
                m_connection->connection().send(Messages::WebRTCResolver::ResolvedAddressError(1), identifier);
            return;
        }

        auto addresses = WTF::map(result.value(), [] (auto& address) {
            return RTCNetwork::IPAddress { rtc::IPAddress { address.getSinAddr() } };
        });

        m_connection->connection().send(Messages::WebRTCResolver::SetResolvedAddress(addresses), identifier);
    });
    resolver->start(address);
    m_resolvers.add(identifier, WTFMove(resolver));
}

void NetworkRTCProvider::stopResolver(uint64_t identifier)
{
    if (auto resolver = m_resolvers.take(identifier))
        resolver->stop();
}

#else

void NetworkRTCProvider::createResolver(uint64_t identifier, const String& address)
{
    auto completionHandler = [this, identifier](WebCore::DNSAddressesOrError&& result) mutable {
        if (!result.has_value()) {
            if (result.error() != WebCore::DNSError::Cancelled)
                m_connection->connection().send(Messages::WebRTCResolver::ResolvedAddressError(1), identifier);
            return;
        }

        auto addresses = WTF::map(result.value(), [] (auto& address) {
            return RTCNetwork::IPAddress { rtc::IPAddress { address.getSinAddr() } };
        });

        m_connection->connection().send(Messages::WebRTCResolver::SetResolvedAddress(addresses), identifier);
    };

    WebCore::resolveDNS(address, identifier, WTFMove(completionHandler));
}

void NetworkRTCProvider::stopResolver(uint64_t identifier)
{
    WebCore::stopResolveDNS(identifier);
}

#endif

void NetworkRTCProvider::closeListeningSockets(Function<void()>&& completionHandler)
{
    if (!m_isListeningSocketAuthorized) {
        completionHandler();
        return;
    }

    m_isListeningSocketAuthorized = false;
    callOnRTCNetworkThread([this, completionHandler = WTFMove(completionHandler)]() mutable {
        Vector<LibWebRTCSocketIdentifier> listeningSocketIdentifiers;
        for (auto& keyValue : m_sockets) {
            if (keyValue.value->type() == Socket::Type::ServerTCP)
                listeningSocketIdentifiers.append(keyValue.key);
        }
        for (auto id : listeningSocketIdentifiers)
            m_sockets.get(id)->close();

        callOnMainThread([provider = makeRef(*this), listeningSocketIdentifiers = WTFMove(listeningSocketIdentifiers), completionHandler = WTFMove(completionHandler)] {
            if (provider->m_connection) {
                for (auto identifier : listeningSocketIdentifiers)
                    provider->m_connection->connection().send(Messages::WebRTCSocket::SignalClose(ECONNABORTED), identifier);
            }
            completionHandler();
        });
    });
}

struct NetworkMessageData : public rtc::MessageData {
    NetworkMessageData(Ref<NetworkRTCProvider>&& rtcProvider, Function<void()>&& callback)
        : rtcProvider(WTFMove(rtcProvider))
        , callback(WTFMove(callback))
    { }
    Ref<NetworkRTCProvider> rtcProvider;
    Function<void()> callback;
};

void NetworkRTCProvider::OnMessage(rtc::Message* message)
{
    ASSERT(message->message_id == 1);
    auto* data = static_cast<NetworkMessageData*>(message->pdata);
    data->callback();
    delete data;
}

void NetworkRTCProvider::callOnRTCNetworkThread(Function<void()>&& callback)
{
    m_rtcNetworkThread->Post(RTC_FROM_HERE, this, 1, new NetworkMessageData(*this, WTFMove(callback)));
}

void NetworkRTCProvider::callSocket(LibWebRTCSocketIdentifier identifier, Function<void(Socket&)>&& callback)
{
    callOnRTCNetworkThread([this, identifier, callback = WTFMove(callback)]() {
        if (auto* socket = m_sockets.get(identifier))
            callback(*socket);
    });
}

void NetworkRTCProvider::sendFromMainThread(Function<void(IPC::Connection&)>&& callback)
{
    callOnMainThread([provider = makeRef(*this), callback = WTFMove(callback)]() {
        if (provider->m_connection)
            callback(provider->m_connection->connection());
    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
