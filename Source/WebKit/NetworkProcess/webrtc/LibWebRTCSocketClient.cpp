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
#include "LibWebRTCSocketClient.h"

#if USE(LIBWEBRTC)

#include "Connection.h"
#include "DataReference.h"
#include "NetworkRTCProvider.h"
#include "WebRTCSocketMessages.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>

namespace WebKit {

LibWebRTCSocketClient::LibWebRTCSocketClient(uint64_t identifier, NetworkRTCProvider& rtcProvider, std::unique_ptr<rtc::AsyncPacketSocket>&& socket, Type type)
    : m_identifier(identifier)
    , m_type(type)
    , m_rtcProvider(rtcProvider)
    , m_socket(WTFMove(socket))
{
    ASSERT(m_socket);

    m_socket->SignalReadPacket.connect(this, &LibWebRTCSocketClient::signalReadPacket);
    m_socket->SignalSentPacket.connect(this, &LibWebRTCSocketClient::signalSentPacket);
    m_socket->SignalClose.connect(this, &LibWebRTCSocketClient::signalClose);

    switch (type) {
    case Type::ServerConnectionTCP:
        return;
    case Type::ClientTCP:
        m_socket->SignalConnect.connect(this, &LibWebRTCSocketClient::signalConnect);
        m_socket->SignalAddressReady.connect(this, &LibWebRTCSocketClient::signalAddressReady);
        return;
    case Type::ServerTCP:
        m_socket->SignalConnect.connect(this, &LibWebRTCSocketClient::signalConnect);
        m_socket->SignalNewConnection.connect(this, &LibWebRTCSocketClient::signalNewConnection);
        signalAddressReady();
        return;
    case Type::UDP:
        m_socket->SignalConnect.connect(this, &LibWebRTCSocketClient::signalConnect);
        signalAddressReady();
        return;
    }
}

void LibWebRTCSocketClient::sendTo(const WebCore::SharedBuffer& buffer, const rtc::SocketAddress& socketAddress, const rtc::PacketOptions& options)
{
    m_socket->SendTo(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size(), socketAddress, options);
}

void LibWebRTCSocketClient::close()
{
    ASSERT(m_socket);
    m_socket->Close();
    m_rtcProvider.takeSocket(m_identifier);
}

void LibWebRTCSocketClient::setOption(int option, int value)
{
    ASSERT(m_socket);
    m_socket->SetOption(static_cast<rtc::Socket::Option>(option), value);
}

void LibWebRTCSocketClient::signalReadPacket(rtc::AsyncPacketSocket* socket, const char* value, size_t length, const rtc::SocketAddress& address, const rtc::PacketTime& packetTime)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    auto buffer = WebCore::SharedBuffer::create(value, length);
    m_rtcProvider.sendFromMainThread([identifier = m_identifier, buffer = WTFMove(buffer), address = RTCNetwork::isolatedCopy(address), packetTime](IPC::Connection& connection) {
        IPC::DataReference data(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
        connection.send(Messages::WebRTCSocket::SignalReadPacket(data, RTCNetwork::IPAddress(address.ipaddr()), address.port(), packetTime), identifier);
    });
}

void LibWebRTCSocketClient::signalSentPacket(rtc::AsyncPacketSocket* socket, const rtc::SentPacket& sentPacket)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_rtcProvider.sendFromMainThread([identifier = m_identifier, sentPacket](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalSentPacket(sentPacket.packet_id, sentPacket.send_time_ms), identifier);
    });
}

void LibWebRTCSocketClient::signalNewConnection(rtc::AsyncPacketSocket* socket, rtc::AsyncPacketSocket* newSocket)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_rtcProvider.newConnection(*this, std::unique_ptr<rtc::AsyncPacketSocket>(newSocket));
}

void LibWebRTCSocketClient::signalAddressReady(rtc::AsyncPacketSocket* socket, const rtc::SocketAddress& address)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_rtcProvider.sendFromMainThread([identifier = m_identifier, address = RTCNetwork::isolatedCopy(address)](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalAddressReady(RTCNetwork::SocketAddress(address)), identifier);
    });
}

void LibWebRTCSocketClient::signalAddressReady()
{
    signalAddressReady(m_socket.get(), m_socket->GetLocalAddress());
}

void LibWebRTCSocketClient::signalConnect(rtc::AsyncPacketSocket* socket)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_rtcProvider.sendFromMainThread([identifier = m_identifier](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalConnect(), identifier);
    });
}

void LibWebRTCSocketClient::signalClose(rtc::AsyncPacketSocket* socket, int error)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_rtcProvider.sendFromMainThread([identifier = m_identifier, error](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalClose(error), identifier);
    });
    // We want to remove 'this' from the socket map now but we will destroy it asynchronously
    // so that the socket parameter of signalClose remains alive as the caller of signalClose may actually being using it afterwards.
    m_rtcProvider.callOnRTCNetworkThread([socket = m_rtcProvider.takeSocket(m_identifier)] { });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
