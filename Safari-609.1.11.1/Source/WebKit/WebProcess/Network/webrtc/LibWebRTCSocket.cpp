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

#include "DataReference.h"
#include "LibWebRTCSocketFactory.h"
#include "NetworkProcessConnection.h"
#include "NetworkRTCSocketMessages.h"
#include "RTCPacketOptions.h"
#include "WebProcess.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>
#include <wtf/MainThread.h>

namespace WebKit {

LibWebRTCSocket::LibWebRTCSocket(LibWebRTCSocketFactory& factory, const void* socketGroup, Type type, const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress)
    : m_factory(factory)
    , m_identifier(WebCore::LibWebRTCSocketIdentifier::generate())
    , m_type(type)
    , m_localAddress(localAddress)
    , m_remoteAddress(remoteAddress)
    , m_socketGroup(socketGroup)
{
    m_factory.addSocket(*this);
}

LibWebRTCSocket::~LibWebRTCSocket()
{
    Close();
    m_factory.removeSocket(*this);
}

void LibWebRTCSocket::sendOnMainThread(Function<void(IPC::Connection&)>&& callback)
{
    callOnMainThread([callback = WTFMove(callback)]() {
        callback(WebProcess::singleton().ensureNetworkProcessConnection().connection());
    });
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

void LibWebRTCSocket::signalReadPacket(const WebCore::SharedBuffer& buffer, rtc::SocketAddress&& address, int64_t timestamp)
{
    if (m_isSuspended)
        return;

    m_remoteAddress = WTFMove(address);
    SignalReadPacket(this, buffer.data(), buffer.size(), m_remoteAddress, timestamp);
}

void LibWebRTCSocket::signalSentPacket(int rtcPacketID, int64_t sendTimeMs)
{
    if (m_beingSentPacketSizes.isEmpty())
        return;

    m_availableSendingBytes += m_beingSentPacketSizes.takeFirst();
    SignalSentPacket(this, rtc::SentPacket(rtcPacketID, sendTimeMs));
    if (m_shouldSignalReadyToSend) {
        m_shouldSignalReadyToSend = false;
        SignalReadyToSend(this);
    }
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

void LibWebRTCSocket::signalNewConnection(rtc::AsyncPacketSocket* newConnectionSocket)
{
    ASSERT(m_type == Type::ServerTCP);
    SignalNewConnection(this, newConnectionSocket);
}

bool LibWebRTCSocket::willSend(size_t size)
{
    if (size > m_availableSendingBytes) {
        m_shouldSignalReadyToSend = true;
        setError(EWOULDBLOCK);
        return false;
    }
    m_availableSendingBytes -= size;
    m_beingSentPacketSizes.append(size);
    return true;
}

int LibWebRTCSocket::SendTo(const void *value, size_t size, const rtc::SocketAddress& address, const rtc::PacketOptions& options)
{
    if (!willSend(size))
        return -1;

    if (m_isSuspended)
        return size;

    auto buffer = WebCore::SharedBuffer::create(static_cast<const uint8_t*>(value), size);
    auto identifier = this->identifier();

    sendOnMainThread([identifier, buffer = WTFMove(buffer), address, options](auto& connection) {
        IPC::DataReference data(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
        connection.send(Messages::NetworkRTCSocket::SendTo { data, RTCNetwork::SocketAddress { address }, RTCPacketOptions { options } }, identifier);
    });
    return size;
}

int LibWebRTCSocket::Close()
{
    if (m_state == STATE_CLOSED)
        return 0;

    m_state = STATE_CLOSED;

    sendOnMainThread([identifier = identifier()](auto& connection) {
        connection.send(Messages::NetworkRTCSocket::Close(), identifier);
    });
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

    sendOnMainThread([identifier = identifier(), option, value](auto& connection) {
        connection.send(Messages::NetworkRTCSocket::SetOption(option, value), identifier);
    });
    return 0;
}

void LibWebRTCSocket::resume()
{
    m_isSuspended = false;

    // On resume, we notify libwebrtc that TCP sockets are errored.
    // We notify libwebrtc that all pending UDP packets have been sent even though we actually dropped them.
    if (m_type != Type::UDP) {
        signalClose(-1);
        return;
    }

    auto currentTime = rtc::TimeMillis();
    while (!m_beingSentPacketSizes.isEmpty())
        signalSentPacket(-1, currentTime);
}

void LibWebRTCSocket::suspend()
{
    m_isSuspended = true;

    // On suspend, we close TCP sockets as we cannot make sure packets are delivered reliably.
    if (m_type != Type::UDP) {
        sendOnMainThread([identifier = identifier()](auto& connection) {
            connection.send(Messages::NetworkRTCSocket::Close { }, identifier);
        });
    }
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
