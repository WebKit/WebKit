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
#include "WebRTCSocket.h"

#if USE(LIBWEBRTC)

#include "DataReference.h"
#include "LibWebRTCSocket.h"
#include "LibWebRTCSocketFactory.h"
#include "NetworkRTCSocketMessages.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>

namespace WebKit {

void WebRTCSocket::signalOnNetworkThread(LibWebRTCSocketFactory& factory, uint64_t identifier, Function<void(LibWebRTCSocket&)>&& callback)
{
    // factory is staying valid during the process lifetime.
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([&factory, identifier, callback = WTFMove(callback)]() {
        auto* socket = factory.socket(identifier);
        if (!socket)
            return;
        callback(*socket);
    });
}

WebRTCSocket::WebRTCSocket(LibWebRTCSocketFactory& factory, uint64_t identifier)
    : m_factory(factory)
    , m_identifier(identifier)
{
}

void WebRTCSocket::signalAddressReady(const RTCNetwork::SocketAddress& address)
{
    signalOnNetworkThread(m_factory, m_identifier, [address](LibWebRTCSocket& socket) {
        socket.signalAddressReady(address.value);
    });
}

void WebRTCSocket::signalReadPacket(const IPC::DataReference& data, const RTCNetwork::IPAddress& address, uint16_t port, int64_t timestamp)
{
    auto buffer = WebCore::SharedBuffer::create(data.data(), data.size());
    signalOnNetworkThread(m_factory, m_identifier, [buffer = WTFMove(buffer), address, port, timestamp](LibWebRTCSocket& socket) {
        socket.signalReadPacket(buffer.get(), rtc::SocketAddress(address.value, port), timestamp);
    });
}

void WebRTCSocket::signalSentPacket(int rtcPacketID, int64_t sendTimeMs)
{
    signalOnNetworkThread(m_factory, m_identifier, [rtcPacketID, sendTimeMs](LibWebRTCSocket& socket) {
        socket.signalSentPacket(rtcPacketID, sendTimeMs);
    });
}

void WebRTCSocket::signalConnect()
{
    signalOnNetworkThread(m_factory, m_identifier, [](LibWebRTCSocket& socket) {
        socket.signalConnect();
    });
}

void WebRTCSocket::signalClose(int error)
{
    signalOnNetworkThread(m_factory, m_identifier, [error](LibWebRTCSocket& socket) {
        socket.signalClose(error);
    });
}

void WebRTCSocket::signalNewConnection(uint64_t newSocketIdentifier, const RTCNetwork::SocketAddress& remoteAddress)
{
    signalOnNetworkThread(m_factory, m_identifier, [newSocketIdentifier, remoteAddress = RTCNetwork::isolatedCopy(remoteAddress.value)](LibWebRTCSocket& socket) {
        socket.signalNewConnection(socket.m_factory.createNewConnectionSocket(socket, newSocketIdentifier, remoteAddress));
    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
