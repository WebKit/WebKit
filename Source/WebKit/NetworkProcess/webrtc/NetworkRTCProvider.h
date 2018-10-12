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

#include "LibWebRTCSocketClient.h"
#include "NetworkRTCMonitor.h"
#include "RTCNetwork.h"
#include <WebCore/LibWebRTCMacros.h>
#include <webrtc/p2p/base/basicpacketsocketfactory.h>
#include <webrtc/rtc_base/third_party/sigslot/sigslot.h>
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {
class NetworkConnectionToWebProcess;
class NetworkRTCResolver;
class NetworkRTCSocket;

class NetworkRTCProvider : public ThreadSafeRefCounted<NetworkRTCProvider>, public rtc::MessageHandler {
public:
    static Ref<NetworkRTCProvider> create(NetworkConnectionToWebProcess& connection) { return adoptRef(*new NetworkRTCProvider(connection)); }
    ~NetworkRTCProvider();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveNetworkRTCMonitorMessage(IPC::Connection& connection, IPC::Decoder& decoder) { m_rtcMonitor.didReceiveMessage(connection, decoder); }
    void didReceiveNetworkRTCSocketMessage(IPC::Connection&, IPC::Decoder&);

    std::unique_ptr<LibWebRTCSocketClient> takeSocket(uint64_t);
    void resolverDone(uint64_t);

    void close();

    void callSocket(uint64_t, Function<void(LibWebRTCSocketClient&)>&&);
    void callOnRTCNetworkThread(Function<void()>&&);
    void sendFromMainThread(Function<void(IPC::Connection&)>&&);

    void newConnection(LibWebRTCSocketClient&, std::unique_ptr<rtc::AsyncPacketSocket>&&);

    void closeListeningSockets(Function<void()>&&);
    void authorizeListeningSockets() { m_isListeningSocketAuthorized = true; }

private:
    explicit NetworkRTCProvider(NetworkConnectionToWebProcess&);

    void createUDPSocket(uint64_t, const RTCNetwork::SocketAddress&, uint16_t, uint16_t);
    void createClientTCPSocket(uint64_t, const RTCNetwork::SocketAddress&, const RTCNetwork::SocketAddress&, int);
    void createServerTCPSocket(uint64_t, const RTCNetwork::SocketAddress&, uint16_t minPort, uint16_t maxPort, int);
    void wrapNewTCPConnection(uint64_t identifier, uint64_t newConnectionSocketIdentifier);

    void createResolver(uint64_t, const String&);
    void stopResolver(uint64_t);

    void addSocket(uint64_t, std::unique_ptr<LibWebRTCSocketClient>&&);

    void createSocket(uint64_t identifier, std::unique_ptr<rtc::AsyncPacketSocket>&&, LibWebRTCSocketClient::Type);

    void OnMessage(rtc::Message*);

    HashMap<uint64_t, std::unique_ptr<NetworkRTCResolver>> m_resolvers;
    HashMap<uint64_t, std::unique_ptr<LibWebRTCSocketClient>> m_sockets;
    NetworkConnectionToWebProcess* m_connection;
    bool m_isStarted { true };

    NetworkRTCMonitor m_rtcMonitor;

    std::unique_ptr<rtc::Thread> m_rtcNetworkThread;
    UniqueRef<rtc::BasicPacketSocketFactory> m_packetSocketFactory;

    HashMap<uint64_t, std::unique_ptr<rtc::AsyncPacketSocket>> m_pendingIncomingSockets;
    uint64_t m_incomingSocketIdentifier { 0 };
    bool m_isListeningSocketAuthorized { true };
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
