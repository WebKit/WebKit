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
    , m_rtcProvider(rtcProvider)
    , m_socket(WTFMove(socket))
{
    if (!m_socket) {
        rtcProvider.sendFromMainThread([identifier](IPC::Connection& connection) {
            connection.send(Messages::WebRTCSocket::SignalClose(1), identifier);
        });
        return;
    }

    m_socket->SignalReadPacket.connect(this, &LibWebRTCSocketClient::signalReadPacket);
    m_socket->SignalSentPacket.connect(this, &LibWebRTCSocketClient::signalSentPacket);
    m_socket->SignalConnect.connect(this, &LibWebRTCSocketClient::signalConnect);
    m_socket->SignalClose.connect(this, &LibWebRTCSocketClient::signalClose);

    if (type == Type::ClientTCP) {
        m_socket->SignalAddressReady.connect(this, &LibWebRTCSocketClient::signalAddressReady);
        return;
    }
    signalAddressReady();
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

void LibWebRTCSocketClient::signalReadPacket(rtc::AsyncPacketSocket*, const char* value, size_t length, const rtc::SocketAddress& address, const rtc::PacketTime& packetTime)
{
    auto buffer = WebCore::SharedBuffer::create(value, length);
    m_rtcProvider.sendFromMainThread([identifier = m_identifier, buffer = WTFMove(buffer), address = RTCNetwork::isolatedCopy(address), packetTime](IPC::Connection& connection) {
        IPC::DataReference data(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
        connection.send(Messages::WebRTCSocket::SignalReadPacket(data, RTCNetwork::IPAddress(address.ipaddr()), address.port(), packetTime.timestamp), identifier);
    });
}

void LibWebRTCSocketClient::signalSentPacket(rtc::AsyncPacketSocket*, const rtc::SentPacket& sentPacket)
{
    m_rtcProvider.sendFromMainThread([identifier = m_identifier, sentPacket](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalSentPacket(sentPacket.packet_id, sentPacket.send_time_ms), identifier);
    });
}

void LibWebRTCSocketClient::signalAddressReady(rtc::AsyncPacketSocket*, const rtc::SocketAddress& address)
{
    m_rtcProvider.sendFromMainThread([identifier = m_identifier, address = RTCNetwork::isolatedCopy(address)](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalAddressReady(RTCNetwork::SocketAddress(address)), identifier);
    });
}

void LibWebRTCSocketClient::signalAddressReady()
{
    signalAddressReady(m_socket.get(), m_socket->GetLocalAddress());
}

void LibWebRTCSocketClient::signalConnect(rtc::AsyncPacketSocket*)
{
    m_rtcProvider.sendFromMainThread([identifier = m_identifier](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalConnect(), identifier);
    });
}

void LibWebRTCSocketClient::signalClose(rtc::AsyncPacketSocket*, int error)
{
    m_rtcProvider.sendFromMainThread([identifier = m_identifier, error](IPC::Connection& connection) {
        connection.send(Messages::WebRTCSocket::SignalClose(error), identifier);
    });
    m_rtcProvider.takeSocket(m_identifier);
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
