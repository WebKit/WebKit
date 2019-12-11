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
#include "LibWebRTCSocketFactory.h"

#if USE(LIBWEBRTC)

#include "NetworkProcessConnection.h"
#include "NetworkRTCMonitorMessages.h"
#include "NetworkRTCProviderMessages.h"
#include "WebProcess.h"
#include "WebRTCSocket.h"
#include <wtf/MainThread.h>

namespace WebKit {
using namespace WebCore;

uint64_t LibWebRTCSocketFactory::s_uniqueResolverIdentifier = 0;

static inline rtc::SocketAddress prepareSocketAddress(const rtc::SocketAddress& address, bool disableNonLocalhostConnections)
{
    auto result = RTCNetwork::isolatedCopy(address);
    if (disableNonLocalhostConnections)
        result.SetIP("127.0.0.1");
    return result;
}

rtc::AsyncPacketSocket* LibWebRTCSocketFactory::createServerTcpSocket(const void* socketGroup, const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort, int options)
{
    auto socket = makeUnique<LibWebRTCSocket>(*this, socketGroup, LibWebRTCSocket::Type::ServerTCP, address, rtc::SocketAddress());

    callOnMainThread([identifier = socket->identifier(), address = prepareSocketAddress(address, m_disableNonLocalhostConnections), minPort, maxPort, options]() {
        if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkRTCProvider::CreateServerTCPSocket(identifier, RTCNetwork::SocketAddress(address), minPort, maxPort, options), 0)) {
            // FIXME: Set error back to socket
            return;
        }

    });
    return socket.release();
}

rtc::AsyncPacketSocket* LibWebRTCSocketFactory::createUdpSocket(const void* socketGroup, const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort)
{
    auto socket = makeUnique<LibWebRTCSocket>(*this, socketGroup, LibWebRTCSocket::Type::UDP, address, rtc::SocketAddress());

    callOnMainThread([identifier = socket->identifier(), address = prepareSocketAddress(address, m_disableNonLocalhostConnections), minPort, maxPort]() {
        if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkRTCProvider::CreateUDPSocket(identifier, RTCNetwork::SocketAddress(address), minPort, maxPort), 0)) {
            // FIXME: Set error back to socket
            return;
        }
    });
    return socket.release();
}

rtc::AsyncPacketSocket* LibWebRTCSocketFactory::createClientTcpSocket(const void* socketGroup, const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress, String&& userAgent, const rtc::PacketSocketTcpOptions& options)
{
    auto socket = makeUnique<LibWebRTCSocket>(*this, socketGroup, LibWebRTCSocket::Type::ClientTCP, localAddress, remoteAddress);
    socket->setState(LibWebRTCSocket::STATE_CONNECTING);

    // FIXME: We only transfer options.opts but should also handle other members.
    callOnMainThread([identifier = socket->identifier(), localAddress = prepareSocketAddress(localAddress, m_disableNonLocalhostConnections), remoteAddress = prepareSocketAddress(remoteAddress, m_disableNonLocalhostConnections), userAgent = WTFMove(userAgent).isolatedCopy(), options = options.opts]() {
        if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkRTCProvider::CreateClientTCPSocket(identifier, RTCNetwork::SocketAddress(localAddress), RTCNetwork::SocketAddress(remoteAddress), userAgent, options), 0)) {
            // FIXME: Set error back to socket
            return;
        }
    });
    return socket.release();
}

rtc::AsyncPacketSocket* LibWebRTCSocketFactory::createNewConnectionSocket(LibWebRTCSocket& serverSocket, LibWebRTCSocketIdentifier newConnectionSocketIdentifier, const rtc::SocketAddress& remoteAddress)
{
    auto socket = makeUnique<LibWebRTCSocket>(*this, serverSocket.socketGroup(), LibWebRTCSocket::Type::ServerConnectionTCP, serverSocket.localAddress(), remoteAddress);
    socket->setState(LibWebRTCSocket::STATE_CONNECTED);

    callOnMainThread([identifier = socket->identifier(), newConnectionSocketIdentifier]() {
        if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkRTCProvider::WrapNewTCPConnection(identifier, newConnectionSocketIdentifier), 0)) {
            // FIXME: Set error back to socket
            return;
        }
    });
    return socket.release();
}

void LibWebRTCSocketFactory::addSocket(LibWebRTCSocket& socket)
{
    ASSERT(!m_sockets.contains(socket.identifier()));
    m_sockets.add(socket.identifier(), &socket);
}

void LibWebRTCSocketFactory::removeSocket(LibWebRTCSocket& socket)
{
    ASSERT(m_sockets.contains(socket.identifier()));
    m_sockets.remove(socket.identifier());
}

void LibWebRTCSocketFactory::forSocketInGroup(const void* socketGroup, const Function<void(LibWebRTCSocket&)>& callback)
{
    for (auto* socket : m_sockets.values()) {
        if (socket->socketGroup() == socketGroup)
            callback(*socket);
    }
}

rtc::AsyncResolverInterface* LibWebRTCSocketFactory::createAsyncResolver()
{
    auto resolver = makeUnique<LibWebRTCResolver>(++s_uniqueResolverIdentifier);
    auto* resolverPointer = resolver.get();
    m_resolvers.set(resolverPointer->identifier(), WTFMove(resolver));
    return resolverPointer;
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
