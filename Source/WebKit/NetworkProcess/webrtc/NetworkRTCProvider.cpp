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

#include "DataReference.h"
#include "LibWebRTCNetworkMessages.h"
#include "LibWebRTCSocketClient.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkRTCProviderMessages.h"
#include "NetworkRTCResolver.h"
#include "NetworkSession.h"
#include "RTCPacketOptions.h"
#include "WebRTCResolverMessages.h"
#include <WebCore/LibWebRTCMacros.h>
#include <webrtc/rtc_base/async_packet_socket.h>
#include <webrtc/rtc_base/logging.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "NetworkRTCResolverCocoa.h"
#include "NetworkRTCTCPSocketCocoa.h"
#include "NetworkRTCUDPSocketCocoa.h"
#endif

namespace WebKit {
using namespace WebCore;

#define RTC_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - NetworkRTCProvider::" fmt, this, ##__VA_ARGS__)
#define RTC_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(Network, "%p - NetworkRTCProvider::" fmt, this, ##__VA_ARGS__)

static rtc::Thread& rtcNetworkThread()
{
    static NeverDestroyed<std::unique_ptr<rtc::Thread>> networkThread;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        networkThread.get() = rtc::Thread::CreateWithSocketServer();
        networkThread.get()->SetName("RTC Network Thread", nullptr);

        auto result = networkThread.get()->Start();
        ASSERT_UNUSED(result, result);
    });
    return *networkThread.get();
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
    , m_ipcConnection(makeRef(connection.connection()))
    , m_rtcMonitor(*this)
    , m_rtcNetworkThread(rtcNetworkThread())
    , m_packetSocketFactory(makeUniqueRefWithoutFastMallocCheck<rtc::BasicPacketSocketFactory>(&m_rtcNetworkThread))
#if PLATFORM(COCOA)
    , m_sourceApplicationAuditToken(connection.networkProcess().sourceApplicationAuditToken())
#endif
{
#if !RELEASE_LOG_DISABLED
    rtc::LogMessage::SetLogOutput(WebKit2LogWebRTC.state == WTFLogChannelState::On ? rtc::LS_INFO : rtc::LS_WARNING, doReleaseLogging);
#endif
}

void NetworkRTCProvider::startListeningForIPC()
{
    m_connection->connection().addThreadMessageReceiver(Messages::NetworkRTCProvider::messageReceiverName(), this);
}

NetworkRTCProvider::~NetworkRTCProvider()
{
    ASSERT(!m_connection);
    ASSERT(!m_sockets.size());
    ASSERT(!m_rtcMonitor.isStarted());
}

void NetworkRTCProvider::close()
{
    RTC_RELEASE_LOG("close");

    // Cancel all pending DNS resolutions.
    while (!m_resolvers.isEmpty())
        stopResolver(*m_resolvers.keys().begin());

    m_connection->connection().removeThreadMessageReceiver(Messages::NetworkRTCProvider::messageReceiverName());
    m_connection = nullptr;
    m_rtcMonitor.stopUpdating();

    callOnRTCNetworkThread([this]() {
        m_sockets.clear();
#if PLATFORM(COCOA)
        m_attributedBundleIdentifiers.clear();
#endif
    });
}

void NetworkRTCProvider::createSocket(LibWebRTCSocketIdentifier identifier, std::unique_ptr<rtc::AsyncPacketSocket>&& socket, Socket::Type type, Ref<IPC::Connection>&& connection)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());
    if (!socket) {
        RTC_RELEASE_LOG_ERROR("createSocket with %u sockets is unable to create a new socket", m_sockets.size());
        connection->send(Messages::LibWebRTCNetwork::SignalClose(identifier, 1), 0);
        return;
    }
    addSocket(identifier, makeUnique<LibWebRTCSocketClient>(identifier, *this, WTFMove(socket), type, WTFMove(connection)));
}

#if PLATFORM(COCOA)
const String& NetworkRTCProvider::attributedBundleIdentifierFromPageIdentifier(WebPageProxyIdentifier pageIdentifier)
{
    return m_attributedBundleIdentifiers.ensure(pageIdentifier, [&]() -> String {
        String value;
        callOnMainRunLoopAndWait([&] {
            auto* session = m_connection->networkSession();
            if (session)
                value = session->attributedBundleIdentifierFromPageIdentifier(pageIdentifier).isolatedCopy();
        });
        return value;
    }).iterator->value;
}
#endif

void NetworkRTCProvider::createUDPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& address, uint16_t minPort, uint16_t maxPort, WebPageProxyIdentifier pageIdentifier, bool isFirstParty, bool isRelayDisabled, WebCore::RegistrableDomain&& domain)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());

#if PLATFORM(COCOA)
    if (m_platformUDPSocketsEnabled) {
        if (auto socket = NetworkRTCUDPSocketCocoa::createUDPSocket(identifier, *this, address.value, minPort, maxPort, m_ipcConnection.copyRef(), String(attributedBundleIdentifierFromPageIdentifier(pageIdentifier)), isFirstParty, isRelayDisabled, WTFMove(domain))) {
            addSocket(identifier, WTFMove(socket));
            return;
        }
    }
#endif

    std::unique_ptr<rtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateUdpSocket(address.value, minPort, maxPort));
    createSocket(identifier, WTFMove(socket), Socket::Type::UDP, m_ipcConnection.copyRef());
}

void NetworkRTCProvider::createServerTCPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& address, uint16_t minPort, uint16_t maxPort, int options)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());
    callOnMainRunLoop([this, protectedThis = makeRef(*this), identifier, address, minPort, maxPort, options] {
        if (!m_connection)
            return;

        if (!m_isListeningSocketAuthorized) {
            m_connection->connection().send(Messages::LibWebRTCNetwork::SignalClose(identifier, 1), 0);
            return;
        }

        callOnRTCNetworkThread([this, identifier, address = RTCNetwork::isolatedCopy(address.value), minPort, maxPort, options]() mutable {
            std::unique_ptr<rtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateServerTcpSocket(address, minPort, maxPort, options));
            createSocket(identifier, WTFMove(socket), Socket::Type::ServerTCP, m_ipcConnection.copyRef());
        });
    });
}

#if !PLATFORM(COCOA)
rtc::ProxyInfo NetworkRTCProvider::proxyInfoFromSession(const RTCNetwork::SocketAddress&, NetworkSession&)
{
    return { };
}
#endif

void NetworkRTCProvider::createClientTCPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& localAddress, const RTCNetwork::SocketAddress& remoteAddress, String&& userAgent, int options, WebPageProxyIdentifier pageIdentifier, bool isRelayDisabled)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());

#if PLATFORM(COCOA)
    if (m_platformTCPSocketsEnabled) {
        if (auto socket = NetworkRTCTCPSocketCocoa::createClientTCPSocket(identifier, *this, remoteAddress.value, options, attributedBundleIdentifierFromPageIdentifier(pageIdentifier), isRelayDisabled, m_ipcConnection.copyRef())) {
            addSocket(identifier, WTFMove(socket));
            return;
        }
    }
#endif

    callOnMainRunLoop([this, protectedThis = makeRef(*this), identifier, localAddress, remoteAddress, userAgent = WTFMove(userAgent).isolatedCopy(), options]() mutable {
        if (!m_connection)
            return;

        auto* session = m_connection->networkSession();
        if (!session) {
            m_connection->connection().send(Messages::LibWebRTCNetwork::SignalClose(identifier, 1), 0);
            return;
        }
        callOnRTCNetworkThread([this, identifier, localAddress = RTCNetwork::isolatedCopy(localAddress.value), remoteAddress = RTCNetwork::isolatedCopy(remoteAddress.value), proxyInfo = proxyInfoFromSession(remoteAddress, *session), userAgent = WTFMove(userAgent).isolatedCopy(), options]() mutable {

            rtc::PacketSocketTcpOptions tcpOptions;
            tcpOptions.opts = options;
            std::unique_ptr<rtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateClientTcpSocket(localAddress, remoteAddress, proxyInfo, userAgent.utf8().data(), tcpOptions));
            createSocket(identifier, WTFMove(socket), Socket::Type::ClientTCP, m_ipcConnection.copyRef());
        });
    });
}

void NetworkRTCProvider::wrapNewTCPConnection(LibWebRTCSocketIdentifier identifier, LibWebRTCSocketIdentifier newConnectionSocketIdentifier)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());
    auto socket = m_pendingIncomingSockets.take(newConnectionSocketIdentifier);
    RELEASE_LOG_IF(!socket, WebRTC, "NetworkRTCProvider::wrapNewTCPConnection received an invalid socket identifier");
    if (socket)
        addSocket(identifier, makeUnique<LibWebRTCSocketClient>(identifier, *this, WTFMove(socket), Socket::Type::ServerConnectionTCP, m_ipcConnection.copyRef()));
}

void NetworkRTCProvider::sendToSocket(LibWebRTCSocketIdentifier identifier, const IPC::DataReference& data, RTCNetwork::SocketAddress&& address, RTCPacketOptions&& options)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());
    auto* socket = m_sockets.get(identifier);
    if (!socket)
        return;

    socket->sendTo(data.data(), data.size(), address.value, options.options);
}

void NetworkRTCProvider::closeSocket(LibWebRTCSocketIdentifier identifier)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());
    auto* socket = m_sockets.get(identifier);
    if (!socket)
        return;
    socket->close();
}

void NetworkRTCProvider::doSocketTaskOnRTCNetworkThread(LibWebRTCSocketIdentifier identifier, Function<void(Socket&)>&& callback)
{
    callOnRTCNetworkThread([this, identifier, callback = WTFMove(callback)]() mutable {
        if (auto* socket = m_sockets.get(identifier))
            callback(*socket);
    });
}

void NetworkRTCProvider::setSocketOption(LibWebRTCSocketIdentifier identifier, int option, int value)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());
    auto* socket = m_sockets.get(identifier);
    if (!socket)
        return;
    socket->setOption(option, value);
}

void NetworkRTCProvider::addSocket(LibWebRTCSocketIdentifier identifier, std::unique_ptr<Socket>&& socket)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());
    m_sockets.add(identifier, WTFMove(socket));
}

std::unique_ptr<NetworkRTCProvider::Socket> NetworkRTCProvider::takeSocket(LibWebRTCSocketIdentifier identifier)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());
    return m_sockets.take(identifier);
}

void NetworkRTCProvider::newConnection(Socket& serverSocket, std::unique_ptr<rtc::AsyncPacketSocket>&& newSocket)
{
    ASSERT(m_rtcNetworkThread.IsCurrent());
    auto incomingSocketIdentifier = LibWebRTCSocketIdentifier::generate();
    m_ipcConnection->send(Messages::LibWebRTCNetwork::SignalNewConnection(serverSocket.identifier(), incomingSocketIdentifier, RTCNetwork::SocketAddress(newSocket->GetRemoteAddress())), 0);
    m_pendingIncomingSockets.add(incomingSocketIdentifier, WTFMove(newSocket));
}

void NetworkRTCProvider::dispatchToThread(Function<void()>&& callback)
{
    callOnRTCNetworkThread((WTFMove(callback)));
}

void NetworkRTCProvider::createResolver(LibWebRTCResolverIdentifier identifier, String&& address)
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([this, protectedThis = makeRef(*this), identifier, address = WTFMove(address).isolatedCopy()]() mutable {
            if (!m_connection)
                return;
            createResolver(identifier, WTFMove(address));
        });
        return;
    }
    WebCore::DNSCompletionHandler completionHandler = [this, identifier](auto&& result) {
        if (!result.has_value()) {
            if (result.error() != WebCore::DNSError::Cancelled)
                m_connection->connection().send(Messages::WebRTCResolver::ResolvedAddressError(1), identifier);
            return;
        }

        Vector<RTCNetwork::IPAddress> ipAddresses;
        ipAddresses.reserveInitialCapacity(result.value().size());
        for (auto& address : result.value()) {
            if (address.isIPv4())
                ipAddresses.uncheckedAppend(rtc::IPAddress { address.ipv4Address() });
            else if (address.isIPv6())
                ipAddresses.uncheckedAppend(rtc::IPAddress { address.ipv6Address() });
        }

        m_connection->connection().send(Messages::WebRTCResolver::SetResolvedAddress(ipAddresses), identifier);
    };

#if PLATFORM(COCOA)
    auto resolver = NetworkRTCResolver::create(identifier, WTFMove(completionHandler));
    resolver->start(address);
    m_resolvers.add(identifier, WTFMove(resolver));
#else
    WebCore::resolveDNS(address, identifier.toUInt64(), WTFMove(completionHandler));
#endif
}

void NetworkRTCProvider::stopResolver(LibWebRTCResolverIdentifier identifier)
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([this, protectedThis = makeRef(*this), identifier] {
            if (!m_connection)
                return;
            stopResolver(identifier);
        });
        return;
    }
#if PLATFORM(COCOA)
    if (auto resolver = m_resolvers.take(identifier))
        resolver->stop();
#else
    WebCore::stopResolveDNS(identifier.toUInt64());
#endif
}

void NetworkRTCProvider::closeListeningSockets(Function<void()>&& completionHandler)
{
    ASSERT(isMainRunLoop());
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

        callOnMainRunLoop([provider = makeRef(*this), listeningSocketIdentifiers = WTFMove(listeningSocketIdentifiers), completionHandler = WTFMove(completionHandler)] {
            if (provider->m_connection) {
                for (auto identifier : listeningSocketIdentifiers)
                    provider->m_connection->connection().send(Messages::LibWebRTCNetwork::SignalClose(identifier, ECONNABORTED), 0);
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
    m_rtcNetworkThread.Post(RTC_FROM_HERE, this, 1, new NetworkMessageData(*this, WTFMove(callback)));
}

#undef RTC_RELEASE_LOG
#undef RTC_RELEASE_LOG_ERROR

} // namespace WebKit

#endif // USE(LIBWEBRTC)
