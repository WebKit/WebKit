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
#include "LibWebRTCSocketFactory.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCNetwork.h"
#include "Logging.h"
#include "NetworkProcessConnection.h"
#include "NetworkRTCProviderMessages.h"
#include "RTCSocketCreationFlags.h"
#include "WebProcess.h"
#include <WebCore/DeprecatedGlobalSettings.h>
#include <wtf/MainThread.h>

namespace WebKit {
using namespace WebCore;

static inline webrtc::SocketAddress prepareSocketAddress(const webrtc::SocketAddress& address, bool disableNonLocalhostConnections)
{
    auto result = address;
    if (disableNonLocalhostConnections)
        result.SetIP("127.0.0.1");
    return result;
}

void LibWebRTCSocketFactory::setConnection(RefPtr<IPC::Connection>&& connection)
{
    ASSERT(!WTF::isMainRunLoop());
    m_connection = connection.copyRef();
    if (!m_connection)
        return;

    while (!m_pendingMessageTasks.isEmpty())
        m_pendingMessageTasks.takeFirst()(*connection);
}

IPC::Connection* LibWebRTCSocketFactory::connection()
{
    ASSERT(!WTF::isMainRunLoop());
    return m_connection.get();
}

std::unique_ptr<webrtc::AsyncPacketSocket> LibWebRTCSocketFactory::createUdpSocket(WebCore::ScriptExecutionContextIdentifier contextIdentifier, const webrtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort, WebPageProxyIdentifier pageIdentifier, RTCSocketCreationFlags flags, const WebCore::RegistrableDomain& domain)
{
    ASSERT(!WTF::isMainRunLoop());
    auto socket = makeUnique<LibWebRTCSocket>(*this, contextIdentifier, LibWebRTCSocket::Type::UDP, address, webrtc::SocketAddress());

    if (RefPtr connection = m_connection)
        connection->send(Messages::NetworkRTCProvider::CreateUDPSocket(socket->identifier(), RTCNetwork::SocketAddress(address), minPort, maxPort, pageIdentifier, flags, domain), 0);
    else {
        callOnMainRunLoop([] {
            WebProcess::singleton().ensureNetworkProcessConnection();
        });
        m_pendingMessageTasks.append([identifier = socket->identifier(), address = RTCNetwork::SocketAddress(address), minPort, maxPort, pageIdentifier, flags, domain](auto& connection) {
            connection.send(Messages::NetworkRTCProvider::CreateUDPSocket(identifier, address, minPort, maxPort, pageIdentifier, flags, domain), 0);
        });
    }

    return socket;
}

std::unique_ptr<webrtc::AsyncPacketSocket> LibWebRTCSocketFactory::createClientTcpSocket(WebCore::ScriptExecutionContextIdentifier contextIdentifier, const webrtc::SocketAddress& localAddress, const webrtc::SocketAddress& remoteAddress, String&& userAgent, const webrtc::PacketSocketTcpOptions& options, WebPageProxyIdentifier pageIdentifier, RTCSocketCreationFlags flags, const WebCore::RegistrableDomain& domain)
{
    ASSERT(!WTF::isMainRunLoop());

    auto socket = makeUnique<LibWebRTCSocket>(*this, contextIdentifier, LibWebRTCSocket::Type::ClientTCP, localAddress, remoteAddress);
    socket->setState(LibWebRTCSocket::STATE_CONNECTING);

    // FIXME: We only transfer options.opts but should also handle other members.
    if (RefPtr connection = m_connection)
        connection->send(Messages::NetworkRTCProvider::CreateClientTCPSocket(socket->identifier(), RTCNetwork::SocketAddress(prepareSocketAddress(localAddress, m_disableNonLocalhostConnections)), RTCNetwork::SocketAddress(prepareSocketAddress(remoteAddress, m_disableNonLocalhostConnections)), userAgent, options.opts, pageIdentifier, flags, domain), 0);
    else {
        callOnMainRunLoop([] {
            WebProcess::singleton().ensureNetworkProcessConnection();
        });
        m_pendingMessageTasks.append([identifier = socket->identifier(), localAddress = RTCNetwork::SocketAddress(prepareSocketAddress(localAddress, m_disableNonLocalhostConnections)), remoteAddress = RTCNetwork::SocketAddress(prepareSocketAddress(remoteAddress, m_disableNonLocalhostConnections)), userAgent, opts = options.opts, pageIdentifier, flags, domain](auto& connection) {
            connection.send(Messages::NetworkRTCProvider::CreateClientTCPSocket(identifier, localAddress, remoteAddress, userAgent, opts, pageIdentifier, flags, domain), 0);
        });
    }

    return socket;
}

void LibWebRTCSocketFactory::addSocket(LibWebRTCSocket& socket)
{
    ASSERT(!WTF::isMainRunLoop());
    ASSERT(!m_sockets.contains(socket.identifier()));
    m_sockets.add(socket.identifier(), socket);
}

void LibWebRTCSocketFactory::removeSocket(LibWebRTCSocket& socket)
{
    ASSERT(!WTF::isMainRunLoop());
    ASSERT(m_sockets.contains(socket.identifier()));
    m_sockets.remove(socket.identifier());
}

void LibWebRTCSocketFactory::forSocketInGroup(ScriptExecutionContextIdentifier contextIdentifier, NOESCAPE const Function<void(LibWebRTCSocket&)>& callback)
{
    ASSERT(!WTF::isMainRunLoop());
    for (CheckedRef socket : m_sockets.values()) {
        if (socket->contextIdentifier() == contextIdentifier)
            callback(socket);
    }
}

std::unique_ptr<LibWebRTCResolver> LibWebRTCSocketFactory::createAsyncDnsResolver()
{
    auto resolver = makeUnique<LibWebRTCResolver>();

    ASSERT(!m_resolvers.contains(resolver->identifier()));
    m_resolvers.add(resolver->identifier(), *resolver);

    return resolver;
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
