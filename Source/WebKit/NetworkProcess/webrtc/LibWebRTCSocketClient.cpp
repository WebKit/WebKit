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

#if !PLATFORM(COCOA)

#if USE(LIBWEBRTC)

#include "Connection.h"
#include "LibWebRTCNetworkMessages.h"
#include "Logging.h"
#include "NetworkRTCProvider.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCSocketClient);

LibWebRTCSocketClient::LibWebRTCSocketClient(WebCore::LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider, std::unique_ptr<webrtc::AsyncPacketSocket>&& socket, Type type, Ref<IPC::Connection>&& connection)
    : m_identifier(identifier)
    , m_type(type)
    , m_rtcProvider(rtcProvider)
    , m_socket(WTFMove(socket))
    , m_connection(WTFMove(connection))
{
    ASSERT(m_socket);

    m_socket->RegisterReceivedPacketCallback([this](auto* socket, auto& packet) {
        signalReadPacket(socket, packet.payload().data(), packet.payload().size(), packet.source_address(), packet.arrival_time()->us_or(0));
    });
    m_socket->SignalSentPacket.connect(this, &LibWebRTCSocketClient::signalSentPacket);
    m_socket->SubscribeCloseEvent(this, [this](webrtc::AsyncPacketSocket* socket, int error) {
        signalClose(socket, error);
    });

    switch (type) {
    case Type::ServerConnectionTCP:
        return;
    case Type::ClientTCP:
        m_socket->SignalConnect.connect(this, &LibWebRTCSocketClient::signalConnect);
        m_socket->SignalAddressReady.connect(this, &LibWebRTCSocketClient::signalAddressReady);
        return;
    case Type::UDP:
        m_socket->SignalConnect.connect(this, &LibWebRTCSocketClient::signalConnect);
        signalAddressReady();
        return;
    }
}

void LibWebRTCSocketClient::sendTo(std::span<const uint8_t> data, const webrtc::SocketAddress& socketAddress, const webrtc::AsyncSocketPacketOptions& options)
{
    m_socket->SendTo(data.data(), data.size(), socketAddress, options);
    auto error = m_socket->GetError();
    RELEASE_LOG_ERROR_IF(error && m_sendError != error, Network, "LibWebRTCSocketClient::sendTo (ID=%" PRIu64 ") failed with error %d", m_identifier.toUInt64(), error);
    m_sendError = error;
}

void LibWebRTCSocketClient::close()
{
    ASSERT(m_socket);
    auto result = m_socket->Close();
    UNUSED_PARAM(result);
    RELEASE_LOG_ERROR_IF(result, Network, "LibWebRTCSocketClient::close (ID=%" PRIu64 ") failed with error %d", m_identifier.toUInt64(), m_socket->GetError());

    m_socket->DeregisterReceivedPacketCallback();
    Ref { m_rtcProvider.get() }->takeSocket(m_identifier);
}

void LibWebRTCSocketClient::setOption(int option, int value)
{
    ASSERT(m_socket);
    auto result = m_socket->SetOption(static_cast<webrtc::Socket::Option>(option), value);
    UNUSED_PARAM(result);
    RELEASE_LOG_ERROR_IF(result, Network, "LibWebRTCSocketClient::setOption(%d, %d) (ID=%" PRIu64 ") failed with error %d", option, value, m_identifier.toUInt64(), m_socket->GetError());
}

void LibWebRTCSocketClient::signalReadPacket(webrtc::AsyncPacketSocket* socket, const unsigned char* value, size_t length, const webrtc::SocketAddress& address, int64_t packetTime)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    std::span data(byteCast<uint8_t>(value), length);
    m_connection->send(Messages::LibWebRTCNetwork::SignalReadPacket(m_identifier, data, RTCNetwork::IPAddress(address.ipaddr()), address.port(), packetTime, WebRTCNetwork::EcnMarking::kNotEct), 0);
}

void LibWebRTCSocketClient::signalSentPacket(webrtc::AsyncPacketSocket* socket, const webrtc::SentPacketInfo& sentPacket)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_connection->send(Messages::LibWebRTCNetwork::SignalSentPacket(m_identifier, sentPacket.packet_id, sentPacket.send_time_ms), 0);
}

void LibWebRTCSocketClient::signalAddressReady(webrtc::AsyncPacketSocket* socket, const webrtc::SocketAddress& address)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_connection->send(Messages::LibWebRTCNetwork::SignalAddressReady(m_identifier, RTCNetwork::SocketAddress(address)), 0);
}

void LibWebRTCSocketClient::signalAddressReady()
{
    signalAddressReady(m_socket.get(), m_socket->GetLocalAddress());
}

void LibWebRTCSocketClient::signalConnect(webrtc::AsyncPacketSocket* socket)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_connection->send(Messages::LibWebRTCNetwork::SignalConnect(m_identifier), 0);
}

void LibWebRTCSocketClient::signalClose(webrtc::AsyncPacketSocket* socket, int error)
{
    ASSERT_UNUSED(socket, m_socket.get() == socket);
    m_connection->send(Messages::LibWebRTCNetwork::SignalClose(m_identifier, error), 0);

    // We want to remove 'this' from the socket map now but we will destroy it asynchronously
    // so that the socket parameter of signalClose remains alive as the caller of signalClose may actually being using it afterwards.
    Ref rtcProvider = m_rtcProvider.get();
    rtcProvider->callOnRTCNetworkThread([socket = rtcProvider->takeSocket(m_identifier)] { });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)

#endif // !PLATFORM(COCOA)
