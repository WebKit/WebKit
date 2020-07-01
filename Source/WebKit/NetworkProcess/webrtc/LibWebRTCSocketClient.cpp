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
#include "LibWebRTCNetworkMessages.h"
#include "Logging.h"
#include "NetworkRTCProvider.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>

namespace WebKit {

LibWebRTCSocketClient::LibWebRTCSocketClient(WebCore::LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, std::unique_ptr<rtc::AsyncPacketSocket>&& socket, Type type, Ref<IPC::Connection>&& connection)
    : m_identifier(identifier)
    , m_type(type)
    , m_rtcProvider(rtcProvider)
    , m_socket(WTFMove(socket))
    , m_connection(WTFMove(connection))
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

void LibWebRTCSocketClient::sendTo(const uint8_t* data, size_t size, const rtc::SocketAddress& socketAddress, const rtc::PacketOptions& options)
{
    auto result = m_socket->SendTo(data, size, socketAddress, options);
    RELEASE_LOG_ERROR_IF(result && m_sendError != result, Network, "LibWebRTCSocketClient::sendTo failed with error %d", m_socket->GetError());
    m_sendError = result;
}

void LibWebRTCSocketClient::close()
{
    ASSERT(m_socket);
    auto result = m_socket->Close();
    UNUSED_PARAM(result);
    RELEASE_LOG_ERROR_IF(result, Network, "LibWebRTCSocketClient::close failed with error %d", m_socket->GetError());

    m_rtcProvider.takeSocket(m_identifier);
}

void LibWebRTCSocketClient::setOption(int option, int value)
{
    ASSERT(m_socket);
    auto result = m_socket->SetOption(static_cast<rtc::Socket::Option>(option), value);
    UNUSED_PARAM(result);
    RELEASE_LOG_ERROR_IF(result, Network, "LibWebRTCSocketClient::setOption(%d, %d) failed with error %d", option, value, m_socket->GetError());
}

void LibWebRTCSocketClient::signalReadPacket(rtc::AsyncPacketSocket* socket, const char* value, size_t length, const rtc::SocketAddress& address, const rtc::PacketTime& packetTime)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    IPC::DataReference data(reinterpret_cast<const uint8_t*>(value), length);
    m_connection->send(Messages::LibWebRTCNetwork::SignalReadPacket(m_identifier, data, RTCNetwork::IPAddress(address.ipaddr()), address.port(), packetTime), 0);
}

void LibWebRTCSocketClient::signalSentPacket(rtc::AsyncPacketSocket* socket, const rtc::SentPacket& sentPacket)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_connection->send(Messages::LibWebRTCNetwork::SignalSentPacket(m_identifier, sentPacket.packet_id, sentPacket.send_time_ms), 0);
}

void LibWebRTCSocketClient::signalNewConnection(rtc::AsyncPacketSocket* socket, rtc::AsyncPacketSocket* newSocket)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_rtcProvider.newConnection(*this, std::unique_ptr<rtc::AsyncPacketSocket>(newSocket));
}

void LibWebRTCSocketClient::signalAddressReady(rtc::AsyncPacketSocket* socket, const rtc::SocketAddress& address)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_connection->send(Messages::LibWebRTCNetwork::SignalAddressReady(m_identifier, RTCNetwork::SocketAddress(address)), 0);
}

void LibWebRTCSocketClient::signalAddressReady()
{
    signalAddressReady(m_socket.get(), m_socket->GetLocalAddress());
}

void LibWebRTCSocketClient::signalConnect(rtc::AsyncPacketSocket* socket)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_connection->send(Messages::LibWebRTCNetwork::SignalConnect(m_identifier), 0);
}

void LibWebRTCSocketClient::signalClose(rtc::AsyncPacketSocket* socket, int error)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_connection->send(Messages::LibWebRTCNetwork::SignalClose(m_identifier, error), 0);

    // We want to remove 'this' from the socket map now but we will destroy it asynchronously
    // so that the socket parameter of signalClose remains alive as the caller of signalClose may actually being using it afterwards.
    m_rtcProvider.callOnRTCNetworkThread([socket = m_rtcProvider.takeSocket(m_identifier)] { });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
