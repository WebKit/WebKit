/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "LibWebRTCNetworkMessages.h"
#include "LibWebRTCSocketClient.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkRTCProviderMessages.h"
#include "NetworkSession.h"
#include "RTCPacketOptions.h"
#include "RTCSocketCreationFlags.h"
#include "WebRTCResolverMessages.h"
#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/LibWebRTCProvider.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "NetworkRTCTCPSocketCocoa.h"
#include "NetworkRTCUDPSocketCocoa.h"
#include "NetworkSessionCocoa.h"
#else // PLATFORM(COCOA)

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/rtc_base/async_packet_socket.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif // PLATFORM(COCOA)

namespace WebKit {
using namespace WebCore;

#define RTC_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - NetworkRTCProvider::" fmt, this, ##__VA_ARGS__)
#define RTC_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(Network, "%p - NetworkRTCProvider::" fmt, this, ##__VA_ARGS__)

NetworkRTCProvider::NetworkRTCProvider(NetworkConnectionToWebProcess& connection)
    : m_connection(&connection)
    , m_ipcConnection(connection.connection())
    , m_rtcMonitor(*this)
    , m_sharedPreferences(connection.sharedPreferencesForWebProcessValue())
#if PLATFORM(COCOA)
    , m_sourceApplicationAuditToken(connection.networkProcess().sourceApplicationAuditToken())
    , m_rtcNetworkThreadQueue(WorkQueue::create("NetworkRTCProvider Queue"_s, WorkQueue::QOS::UserInitiated))
#else
    , m_packetSocketFactory(makeUniqueRefWithoutFastMallocCheck<webrtc::BasicPacketSocketFactory>(rtcNetworkThread().socketserver()))
#endif
{
#if PLATFORM(COCOA)
    if (CheckedPtr session = downcast<NetworkSessionCocoa>(connection.networkSession()))
        m_applicationBundleIdentifier = session->sourceApplicationBundleIdentifier().utf8();
#endif
#if !RELEASE_LOG_DISABLED
    LibWebRTCProvider::setRTCLogging(WebKit2LogWebRTC.state == WTFLogChannelState::On ? WTFLogLevel::Info : WTFLogLevel::Warning);
#endif
}

void NetworkRTCProvider::startListeningForIPC()
{
    protectedConnection()->addMessageReceiver(*this, *this, Messages::NetworkRTCProvider::messageReceiverName());
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

    protectedConnection()->removeMessageReceiver(Messages::NetworkRTCProvider::messageReceiverName());
    m_connection = nullptr;
    protectedRTCMonitor()->stopUpdating();

    callOnRTCNetworkThread([this, protectedThis = Ref { *this }] {
        auto sockets = std::exchange(m_sockets, { });
        for (auto& socket : sockets)
            socket.second->close();
        ASSERT(m_sockets.empty());
#if PLATFORM(COCOA)
        m_attributedBundleIdentifiers.clear();
#endif
    });
}

void NetworkRTCProvider::sendToSocket(LibWebRTCSocketIdentifier identifier, std::span<const uint8_t> data, RTCNetwork::SocketAddress&& address, RTCPacketOptions&& options)
{
    assertIsRTCNetworkThread();
    auto iterator = m_sockets.find(identifier);
    if (iterator == m_sockets.end())
        return;
    iterator->second->sendTo(data, address.rtcAddress(), options.options);
}

void NetworkRTCProvider::closeSocket(LibWebRTCSocketIdentifier identifier)
{
    assertIsRTCNetworkThread();
    auto iterator = m_sockets.find(identifier);
    if (iterator == m_sockets.end())
        return;
    iterator->second->close();
}

void NetworkRTCProvider::setSocketOption(LibWebRTCSocketIdentifier identifier, int option, int value)
{
    assertIsRTCNetworkThread();
    auto iterator = m_sockets.find(identifier);
    if (iterator == m_sockets.end())
        return;
    iterator->second->setOption(option, value);
}

void NetworkRTCProvider::addSocket(LibWebRTCSocketIdentifier identifier, std::unique_ptr<Socket>&& socket)
{
    assertIsRTCNetworkThread();
    ASSERT(socket);
    ASSERT(!m_sockets.contains(identifier));
    m_sockets.emplace(identifier, WTFMove(socket));

    RTC_RELEASE_LOG("new socket %" PRIu64 ", total socket number is %lu", identifier.toUInt64(), m_sockets.size());
    if (m_sockets.size() > maxSockets) {
        auto socketIdentifierToClose = m_sockets.begin()->first;
        RTC_RELEASE_LOG_ERROR("too many sockets, closing %" PRIu64, socketIdentifierToClose.toUInt64());
        closeSocket(socketIdentifierToClose);
        ASSERT(m_sockets.find(socketIdentifierToClose) == m_sockets.end());
    }
}

std::unique_ptr<NetworkRTCProvider::Socket> NetworkRTCProvider::takeSocket(LibWebRTCSocketIdentifier identifier)
{
    assertIsRTCNetworkThread();
    auto iterator = m_sockets.find(identifier);
    if (iterator == m_sockets.end())
        return nullptr;

    auto socket = WTFMove(iterator->second);
    m_sockets.erase(iterator);
    return socket;
}

void NetworkRTCProvider::dispatch(Function<void()>&& callback)
{
    callOnRTCNetworkThread((WTFMove(callback)));
}

void NetworkRTCProvider::createResolver(LibWebRTCResolverIdentifier identifier, String&& address)
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([this, protectedThis = Ref { *this }, identifier, address = WTFMove(address).isolatedCopy()]() mutable {
            if (!m_connection)
                return;
            createResolver(identifier, WTFMove(address));
        });
        return;
    }

    RefPtr connection = m_connection.get();
    if (connection && connection->protectedMDNSRegister()->hasRegisteredName(address)) {
        Vector<WebKit::WebRTCNetwork::IPAddress> ipAddresses;
        Ref rtcMonitor = m_rtcMonitor;
        if (!rtcMonitor->ipv4().isUnspecified())
            ipAddresses.append(rtcMonitor->ipv4());
        if (!rtcMonitor->ipv6().isUnspecified())
            ipAddresses.append(rtcMonitor->ipv6());
        protectedConnection()->send(Messages::WebRTCResolver::SetResolvedAddress(ipAddresses), identifier);
        return;
    }

    WebCore::DNSCompletionHandler completionHandler = [connection = m_connection, identifier](auto&& result) {
        ASSERT(isMainRunLoop());
        if (!connection)
            return;
        RefPtr protectedConnection = connection.get();

        if (!result.has_value()) {
            if (result.error() != WebCore::DNSError::Cancelled)
                protectedConnection->connection().send(Messages::WebRTCResolver::ResolvedAddressError(1), identifier);
            return;
        }

        auto ipAddresses = WTF::compactMap(result.value(), [](auto& address) -> std::optional<RTCNetwork::IPAddress> {
            if (address.isIPv4())
                // FIXME: Remove SUPPRESS_MEMORY_UNSAFE_CAST once rdar://144236356 is fixed.
                SUPPRESS_MEMORY_UNSAFE_CAST return RTCNetwork::IPAddress { webrtc::IPAddress { address.ipv4Address() } };
            if (address.isIPv6())
                // FIXME: Remove SUPPRESS_MEMORY_UNSAFE_CAST once rdar://144236356 is fixed.
                SUPPRESS_MEMORY_UNSAFE_CAST return RTCNetwork::IPAddress { webrtc::IPAddress { address.ipv6Address() } };
            return std::nullopt;
        });

        protectedConnection->connection().send(Messages::WebRTCResolver::SetResolvedAddress(ipAddresses), identifier);
    };

    WebCore::resolveDNS(address, identifier.toUInt64(), WTFMove(completionHandler));
}

void NetworkRTCProvider::stopResolver(LibWebRTCResolverIdentifier identifier)
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([this, protectedThis = Ref { *this }, identifier] {
            if (!m_connection)
                return;
            stopResolver(identifier);
        });
        return;
    }
    WebCore::stopResolveDNS(identifier.toUInt64());
}

#if PLATFORM(COCOA)
bool NetworkRTCProvider::webRTCInterfaceMonitoringViaNWEnabled() const
{
    RefPtr connection = m_connection.get();
    return connection && connection->webRTCInterfaceMonitoringViaNWEnabled();
}

const String& NetworkRTCProvider::attributedBundleIdentifierFromPageIdentifier(WebPageProxyIdentifier pageIdentifier)
{
    return m_attributedBundleIdentifiers.ensure(pageIdentifier, [protectedThis = Ref { *this }, pageIdentifier]() -> String {
        String value;
        callOnMainRunLoopAndWait([protectedThis, &value, pageIdentifier] {
            RefPtr connection = protectedThis->m_connection.get();
            if (CheckedPtr session = connection ? connection->networkSession() : nullptr)
                value = session->attributedBundleIdentifierFromPageIdentifier(pageIdentifier).isolatedCopy();
        });
        return value;
    }).iterator->value;
}

void NetworkRTCProvider::createUDPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& address, uint16_t minPort, uint16_t maxPort, WebPageProxyIdentifier pageIdentifier, RTCSocketCreationFlags flags, WebCore::RegistrableDomain&& domain)
{
    assertIsRTCNetworkThread();

    if (m_sockets.contains(identifier)) {
        RELEASE_LOG_ERROR(WebRTC, "NetworkRTCProvider::createUDPSocket duplicate identifier");
        return;
    }

    auto socket = makeUnique<NetworkRTCUDPSocketCocoa>(identifier, *this, address.rtcAddress(), m_ipcConnection.copyRef(), String(attributedBundleIdentifierFromPageIdentifier(pageIdentifier)), flags, WTFMove(domain));
    addSocket(identifier, WTFMove(socket));
}

void NetworkRTCProvider::createClientTCPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& localAddress, const RTCNetwork::SocketAddress& remoteAddress, String&& userAgent, int options, WebPageProxyIdentifier pageIdentifier, RTCSocketCreationFlags flags, WebCore::RegistrableDomain&& domain)
{
    assertIsRTCNetworkThread();

    if (m_sockets.contains(identifier)) {
        RELEASE_LOG_ERROR(WebRTC, "NetworkRTCProvider::createClientTCPSocket duplicate identifier");
        return;
    }

    auto socket = NetworkRTCTCPSocketCocoa::createClientTCPSocket(identifier, *this, remoteAddress.rtcAddress(), options, attributedBundleIdentifierFromPageIdentifier(pageIdentifier), flags, domain, m_ipcConnection.copyRef());
    if (socket)
        addSocket(identifier, WTFMove(socket));
    else
        signalSocketIsClosed(identifier);
}

void NetworkRTCProvider::getInterfaceName(URL&& url, WebPageProxyIdentifier pageIdentifier, RTCSocketCreationFlags flags, WebCore::RegistrableDomain&& domain, CompletionHandler<void(String&&)>&& completionHandler)
{
    if (!url.protocolIsInHTTPFamily()) {
        completionHandler({ });
        return;
    }

    NetworkRTCTCPSocketCocoa::getInterfaceName(*this, url, attributedBundleIdentifierFromPageIdentifier(pageIdentifier), flags, domain)->whenSettled(m_rtcNetworkThreadQueue, [completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        completionHandler(result ? WTFMove(result.value()) : String { });
    });
}

void NetworkRTCProvider::callOnRTCNetworkThread(Function<void()>&& callback)
{
    m_rtcNetworkThreadQueue->dispatch(WTFMove(callback));
}

void NetworkRTCProvider::assertIsRTCNetworkThread()
{
    assertIsCurrent(m_rtcNetworkThreadQueue);
}

#else // PLATFORM(COCOA)
webrtc::Thread& NetworkRTCProvider::rtcNetworkThread()
{
    static NeverDestroyed<std::unique_ptr<webrtc::Thread>> networkThread = [] {
        auto networkThread = webrtc::Thread::CreateWithSocketServer();
        networkThread->SetName("RTC Network Thread", nullptr);

        auto result = networkThread->Start();
        ASSERT_UNUSED(result, result);
        return networkThread;
    }();
    return *networkThread.get();
}

void NetworkRTCProvider::createUDPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& address, uint16_t minPort, uint16_t maxPort, WebPageProxyIdentifier pageIdentifier, RTCSocketCreationFlags, WebCore::RegistrableDomain&& domain)
{
    assertIsRTCNetworkThread();

    std::unique_ptr<webrtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateUdpSocket(address.rtcAddress(), minPort, maxPort));
    createSocket(identifier, WTFMove(socket), Socket::Type::UDP, m_ipcConnection.copyRef());
}

void NetworkRTCProvider::createClientTCPSocket(LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& localAddress, const RTCNetwork::SocketAddress& remoteAddress, String&& userAgent, int options, WebPageProxyIdentifier pageIdentifier, RTCSocketCreationFlags, WebCore::RegistrableDomain&& domain)
{
    assertIsRTCNetworkThread();

    if (m_sockets.contains(identifier)) {
        RELEASE_LOG_ERROR(WebRTC, "NetworkRTCProvider::createClientTCPSocket duplicate identifier");
        return;
    }

    callOnMainRunLoop([this, protectedThis = Ref { *this }, identifier, localAddress, remoteAddress, userAgent = WTFMove(userAgent).isolatedCopy(), options]() mutable {
        if (!m_connection)
            return;

        if (!m_connection->networkSession()) {
            signalSocketIsClosed(identifier);
            return;
        }
        callOnRTCNetworkThread([this, protectedThis = Ref { *this }, identifier, localAddress = localAddress.rtcAddress(), remoteAddress = remoteAddress.rtcAddress(), options]() mutable {

            if (m_sockets.contains(identifier)) {
                RELEASE_LOG_ERROR(WebRTC, "NetworkRTCProvider::createClientTCPSocket duplicate identifier");
                return;
            }

            webrtc::PacketSocketTcpOptions tcpOptions;
            tcpOptions.opts = options;
            std::unique_ptr<webrtc::AsyncPacketSocket> socket(m_packetSocketFactory->CreateClientTcpSocket(localAddress, remoteAddress, tcpOptions));
            createSocket(identifier, WTFMove(socket), Socket::Type::ClientTCP, m_ipcConnection.copyRef());
        });
    });
}

void NetworkRTCProvider::createSocket(LibWebRTCSocketIdentifier identifier, std::unique_ptr<webrtc::AsyncPacketSocket>&& socket, Socket::Type type, Ref<IPC::Connection>&& connection)
{
    assertIsRTCNetworkThread();
    if (!socket) {
        RTC_RELEASE_LOG_ERROR("createSocket with %lu sockets is unable to create a new socket", m_sockets.size());
        connection->send(Messages::LibWebRTCNetwork::SignalClose(identifier, 1), 0);
        return;
    }
    addSocket(identifier, makeUnique<LibWebRTCSocketClient>(identifier, *this, WTFMove(socket), type, WTFMove(connection)));
}

void NetworkRTCProvider::callOnRTCNetworkThread(Function<void()>&& callback)
{
    rtcNetworkThread().PostTask(WTFMove(callback));
}

void NetworkRTCProvider::assertIsRTCNetworkThread()
{
    ASSERT(rtcNetworkThread().IsCurrent());
}
#endif // !PLATFORM(COCOA)

void NetworkRTCProvider::signalSocketIsClosed(LibWebRTCSocketIdentifier identifier)
{
    protectedConnection()->send(Messages::LibWebRTCNetwork::SignalClose(identifier, 1), 0);
}

Ref<NetworkRTCMonitor> NetworkRTCProvider::protectedRTCMonitor()
{
    return m_rtcMonitor;
}

std::optional<SharedPreferencesForWebProcess> NetworkRTCProvider::sharedPreferencesForWebProcess(IPC::Connection& connection)
{
    Locker locker { m_sharedPreferencesLock };
    return m_sharedPreferences;
}

void NetworkRTCProvider::updateSharedPreferencesForWebProcess(const SharedPreferencesForWebProcess& preferences)
{
    Locker locker { m_sharedPreferencesLock };
    m_sharedPreferences = preferences;
}

#undef RTC_RELEASE_LOG
#undef RTC_RELEASE_LOG_ERROR

} // namespace WebKit

#endif // USE(LIBWEBRTC)
