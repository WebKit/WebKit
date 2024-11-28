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
#include "LibWebRTCSocket.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCNetworkManager.h"
#include "LibWebRTCSocketFactory.h"
#include "NetworkProcessConnection.h"
#include "NetworkRTCProviderMessages.h"
#include "RTCPacketOptions.h"
#include "WebProcess.h"
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCSocket);

LibWebRTCSocket::LibWebRTCSocket(LibWebRTCSocketFactory& factory, WebCore::ScriptExecutionContextIdentifier contextIdentifier, Type type, const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress)
    : m_factory(factory)
    , m_type(type)
    , m_localAddress(localAddress)
    , m_remoteAddress(remoteAddress)
    , m_contextIdentifier(contextIdentifier)
{
    m_factory->addSocket(*this);
}

LibWebRTCSocket::~LibWebRTCSocket()
{
    Close();
    m_factory->removeSocket(*this);
}

rtc::SocketAddress LibWebRTCSocket::GetLocalAddress() const
{
    return m_localAddress;
}

rtc::SocketAddress LibWebRTCSocket::GetRemoteAddress() const
{
    return m_remoteAddress;
}

void LibWebRTCSocket::signalAddressReady(const rtc::SocketAddress& address)
{
    m_localAddress = address;
    m_state = (m_type == Type::ClientTCP) ? STATE_CONNECTED : STATE_BOUND;
    SignalAddressReady(this, m_localAddress);
}

void LibWebRTCSocket::signalReadPacket(std::span<const uint8_t> data, rtc::SocketAddress&& address, int64_t timestamp, rtc::EcnMarking ecn)
{
    if (m_isSuspended)
        return;

    m_remoteAddress = WTFMove(address);
    absl::optional<webrtc::Timestamp> packetTimestamp;
    if (timestamp)
        packetTimestamp = webrtc::Timestamp::Micros(timestamp);
    NotifyPacketReceived({ { data.data(), data.size() }, m_remoteAddress, packetTimestamp, ecn });
}

void LibWebRTCSocket::signalSentPacket(int64_t rtcPacketID, int64_t sendTimeMs)
{
    SignalSentPacket(this, rtc::SentPacket(rtcPacketID, sendTimeMs));
}

void LibWebRTCSocket::signalConnect()
{
    m_state = STATE_CONNECTED;
    SignalConnect(this);
}

void LibWebRTCSocket::signalClose(int error)
{
    m_state = STATE_CLOSED;
    SignalClose(this, error);
}

void LibWebRTCSocket::signalUsedInterface(String&& name)
{
    LibWebRTCNetworkManager::signalUsedInterface(m_contextIdentifier, WTFMove(name));
}

int LibWebRTCSocket::SendTo(const void *value, size_t size, const rtc::SocketAddress& address, const rtc::PacketOptions& options)
{
    RefPtr connection = m_factory->connection();
    if (!connection)
        return -1;

    if (m_isSuspended)
        return size;

    auto data = unsafeMakeSpan(static_cast<const uint8_t*>(value), size);
    connection->send(Messages::NetworkRTCProvider::SendToSocket { identifier(), data, RTCNetwork::SocketAddress { address }, RTCPacketOptions { options } }, 0);

    return size;
}

int LibWebRTCSocket::Close()
{
    RefPtr connection = m_factory->connection();
    if (!connection || m_state == STATE_CLOSED)
        return 0;

    m_state = STATE_CLOSED;

    connection->send(Messages::NetworkRTCProvider::CloseSocket { identifier() }, 0);

    return 0;
}

int LibWebRTCSocket::GetOption(rtc::Socket::Option option, int* value)
{
    ASSERT(option < MAX_SOCKET_OPTION);
    if (auto storedValue = m_options[option]) {
        *value = *storedValue;
        return 0;
    }
    return -1;
}

int LibWebRTCSocket::SetOption(rtc::Socket::Option option, int value)
{
    ASSERT(option < MAX_SOCKET_OPTION);

    m_options[option] = value;

    if (RefPtr connection = m_factory->connection())
        connection->send(Messages::NetworkRTCProvider::SetSocketOption { identifier(), option, value }, 0);

    return 0;
}

void LibWebRTCSocket::resume()
{
    m_isSuspended = false;
}

void LibWebRTCSocket::suspend()
{
    m_isSuspended = true;

    if (m_state == STATE_CLOSED)
        return;

    signalClose(-1);
    if (RefPtr connection = m_factory->connection())
        connection->send(Messages::NetworkRTCProvider::CloseSocket { identifier() }, 0);
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
