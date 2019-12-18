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

#include "NetworkRTCMonitor.h"
#include "RTCNetwork.h"
#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/LibWebRTCSocketIdentifier.h>
#include <webrtc/p2p/base/basic_packet_socket_factory.h>
#include <webrtc/rtc_base/third_party/sigslot/sigslot.h>
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace rtc {
class SocketAddress;
struct PacketOptions;
}

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {
class NetworkConnectionToWebProcess;
class NetworkRTCResolver;
class NetworkRTCSocket;
class NetworkSession;

class NetworkRTCProvider : public ThreadSafeRefCounted<NetworkRTCProvider>, public rtc::MessageHandler {
public:
    static Ref<NetworkRTCProvider> create(NetworkConnectionToWebProcess& connection) { return adoptRef(*new NetworkRTCProvider(connection)); }
    ~NetworkRTCProvider();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveNetworkRTCMonitorMessage(IPC::Connection& connection, IPC::Decoder& decoder) { m_rtcMonitor.didReceiveMessage(connection, decoder); }
    void didReceiveNetworkRTCSocketMessage(IPC::Connection&, IPC::Decoder&);

    class Socket {
    public:
        virtual ~Socket() = default;

        enum class Type : uint8_t { UDP, ServerTCP, ClientTCP, ServerConnectionTCP };
        virtual Type type() const  = 0;
        virtual WebCore::LibWebRTCSocketIdentifier identifier() const = 0;

        virtual void close() = 0;
        virtual void setOption(int option, int value) = 0;
        virtual void sendTo(const WebCore::SharedBuffer&, const rtc::SocketAddress&, const rtc::PacketOptions&) = 0;
    };

    std::unique_ptr<Socket> takeSocket(WebCore::LibWebRTCSocketIdentifier);
    void resolverDone(uint64_t);

    void close();

    void callSocket(WebCore::LibWebRTCSocketIdentifier, Function<void(Socket&)>&&);
    void callOnRTCNetworkThread(Function<void()>&&);
    void sendFromMainThread(Function<void(IPC::Connection&)>&&);

    void newConnection(Socket&, std::unique_ptr<rtc::AsyncPacketSocket>&&);

    void closeListeningSockets(Function<void()>&&);
    void authorizeListeningSockets() { m_isListeningSocketAuthorized = true; }

    bool canLog() const { return m_canLog; }

private:
    explicit NetworkRTCProvider(NetworkConnectionToWebProcess&);

    void createUDPSocket(WebCore::LibWebRTCSocketIdentifier, const RTCNetwork::SocketAddress&, uint16_t, uint16_t);
    void createClientTCPSocket(WebCore::LibWebRTCSocketIdentifier, const RTCNetwork::SocketAddress&, const RTCNetwork::SocketAddress&, String&& userAgent, int);
    void createServerTCPSocket(WebCore::LibWebRTCSocketIdentifier, const RTCNetwork::SocketAddress&, uint16_t minPort, uint16_t maxPort, int);
    void wrapNewTCPConnection(WebCore::LibWebRTCSocketIdentifier identifier, WebCore::LibWebRTCSocketIdentifier newConnectionSocketIdentifier);

    void createResolver(uint64_t, const String&);
    void stopResolver(uint64_t);

    void addSocket(WebCore::LibWebRTCSocketIdentifier, std::unique_ptr<Socket>&&);

    void createSocket(WebCore::LibWebRTCSocketIdentifier, std::unique_ptr<rtc::AsyncPacketSocket>&&, Socket::Type);

    void OnMessage(rtc::Message*);

    static rtc::ProxyInfo proxyInfoFromSession(const RTCNetwork::SocketAddress&, NetworkSession&);

    HashMap<uint64_t, std::unique_ptr<NetworkRTCResolver>> m_resolvers;
    HashMap<WebCore::LibWebRTCSocketIdentifier, std::unique_ptr<Socket>> m_sockets;
    NetworkConnectionToWebProcess* m_connection;
    bool m_isStarted { true };

    NetworkRTCMonitor m_rtcMonitor;

    std::unique_ptr<rtc::Thread> m_rtcNetworkThread;
    UniqueRef<rtc::BasicPacketSocketFactory> m_packetSocketFactory;

    HashMap<WebCore::LibWebRTCSocketIdentifier, std::unique_ptr<rtc::AsyncPacketSocket>> m_pendingIncomingSockets;
    bool m_isListeningSocketAuthorized { true };
    bool m_canLog { false };
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
