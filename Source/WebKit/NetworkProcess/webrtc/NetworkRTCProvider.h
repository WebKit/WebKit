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

#pragma once

#if USE(LIBWEBRTC)

#include "Connection.h"
#include "LibWebRTCResolverIdentifier.h"
#include "NetworkRTCMonitor.h"
#include "RTCNetwork.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/LibWebRTCSocketIdentifier.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/HashMap.h>
#include <wtf/StdMap.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

#if !PLATFORM(COCOA)

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/p2p/base/basic_packet_socket_factory.h>
#include <webrtc/rtc_base/third_party/sigslot/sigslot.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#endif

namespace IPC {
class Connection;
class Decoder;
}

namespace rtc {
class SocketAddress;
struct PacketOptions;
}

namespace WebCore {
class RegistrableDomain;
class FragmentedSharedBuffer;
}

namespace WebKit {
class NetworkConnectionToWebProcess;
class NetworkSession;
struct RTCPacketOptions;

struct SocketComparator {
    bool operator()(const WebCore::LibWebRTCSocketIdentifier& a, const WebCore::LibWebRTCSocketIdentifier& b) const
    {
        return a.toUInt64() < b.toUInt64();
    }
};

class NetworkRTCProvider : private FunctionDispatcher, private IPC::MessageReceiver, public ThreadSafeRefCounted<NetworkRTCProvider, WTF::DestructionThread::MainRunLoop>, public CanMakeThreadSafeCheckedPtr<NetworkRTCProvider> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(NetworkRTCProvider);
public:
    static Ref<NetworkRTCProvider> create(NetworkConnectionToWebProcess& connection)
    {
        auto instance = adoptRef(*new NetworkRTCProvider(connection));
        instance->startListeningForIPC();
        return instance;
    }
    ~NetworkRTCProvider();

    void didReceiveNetworkRTCMonitorMessage(IPC::Connection& connection, IPC::Decoder& decoder) { protectedRTCMonitor()->didReceiveMessage(connection, decoder); }

    class Socket {
    public:
        virtual ~Socket() = default;

        enum class Type : uint8_t { UDP, ClientTCP, ServerConnectionTCP };
        virtual Type type() const  = 0;
        virtual WebCore::LibWebRTCSocketIdentifier identifier() const = 0;

        virtual void close() = 0;
        virtual void setOption(int option, int value) = 0;
        virtual void sendTo(std::span<const uint8_t>, const rtc::SocketAddress&, const rtc::PacketOptions&) = 0;
    };

    std::unique_ptr<Socket> takeSocket(WebCore::LibWebRTCSocketIdentifier);
    void resolverDone(uint64_t);

    void close();

    void callOnRTCNetworkThread(Function<void()>&&);

    IPC::Connection& connection() { return m_ipcConnection.get(); }
    Ref<IPC::Connection> protectedConnection() { return m_ipcConnection.get(); }

    void closeSocket(WebCore::LibWebRTCSocketIdentifier);

#if PLATFORM(COCOA)
    const std::optional<audit_token_t>& sourceApplicationAuditToken() const { return m_sourceApplicationAuditToken; }
    const char* applicationBundleIdentifier() const { return m_applicationBundleIdentifier.data(); }
#endif

private:
    explicit NetworkRTCProvider(NetworkConnectionToWebProcess&);
    void startListeningForIPC();

    void createUDPSocket(WebCore::LibWebRTCSocketIdentifier, const RTCNetwork::SocketAddress&, uint16_t, uint16_t, WebPageProxyIdentifier, bool isFirstParty, bool isRelayDisabled, WebCore::RegistrableDomain&&);
    void createClientTCPSocket(WebCore::LibWebRTCSocketIdentifier, const RTCNetwork::SocketAddress&, const RTCNetwork::SocketAddress&, String&& userAgent, int, WebPageProxyIdentifier, bool isFirstParty, bool isRelayDisabled, WebCore::RegistrableDomain&&);
    void sendToSocket(WebCore::LibWebRTCSocketIdentifier, std::span<const uint8_t>, RTCNetwork::SocketAddress&&, RTCPacketOptions&&);
    void setSocketOption(WebCore::LibWebRTCSocketIdentifier, int option, int value);

    void createResolver(LibWebRTCResolverIdentifier, String&&);
    void stopResolver(LibWebRTCResolverIdentifier);
    void getInterfaceName(URL&&, WebPageProxyIdentifier, bool isFirstParty, bool isRelayDisabled, WebCore::RegistrableDomain&&, CompletionHandler<void(String&&)>&&);

    void addSocket(WebCore::LibWebRTCSocketIdentifier, std::unique_ptr<Socket>&&);

#if PLATFORM(COCOA)
    const String& attributedBundleIdentifierFromPageIdentifier(WebPageProxyIdentifier);
#else
    static rtc::Thread& rtcNetworkThread();
    void createSocket(WebCore::LibWebRTCSocketIdentifier, std::unique_ptr<rtc::AsyncPacketSocket>&&, Socket::Type, Ref<IPC::Connection>&&);
#endif

    // FunctionDispatcher
    void dispatch(Function<void()>&&) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void signalSocketIsClosed(WebCore::LibWebRTCSocketIdentifier);

    void assertIsRTCNetworkThread();

    Ref<NetworkRTCMonitor> protectedRTCMonitor();

#if PLATFORM(COCOA)
    Ref<WorkQueue> protectedRTCNetworkThreadQueue();
#endif

    static constexpr size_t maxSockets { 256 };

    StdMap<WebCore::LibWebRTCSocketIdentifier, std::unique_ptr<Socket>, SocketComparator> m_sockets;
    WeakPtr<NetworkConnectionToWebProcess> m_connection;
    Ref<IPC::Connection> m_ipcConnection;
    bool m_isStarted { true };

    NetworkRTCMonitor m_rtcMonitor;

#if PLATFORM(COCOA)
    HashMap<WebPageProxyIdentifier, String> m_attributedBundleIdentifiers;
    std::optional<audit_token_t> m_sourceApplicationAuditToken;
    CString m_applicationBundleIdentifier;
    Ref<WorkQueue> m_rtcNetworkThreadQueue;
#endif

#if !PLATFORM(COCOA)
    UniqueRef<rtc::BasicPacketSocketFactory> m_packetSocketFactory;
#endif
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
