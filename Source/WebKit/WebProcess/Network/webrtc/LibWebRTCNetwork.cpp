/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "LibWebRTCNetwork.h"

#include "DataReference.h"
#include "LibWebRTCNetworkMessages.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/MainThread.h>

namespace WebKit {

LibWebRTCNetwork::~LibWebRTCNetwork()
{
    ASSERT_NOT_REACHED();
}

void LibWebRTCNetwork::setAsActive()
{
    ASSERT(!m_isActive);
    m_isActive = true;
#if USE(LIBWEBRTC)
    if (m_connection)
        setSocketFactoryConnection();
#endif
}

void LibWebRTCNetwork::networkProcessCrashed()
{
    setConnection(nullptr);

#if USE(LIBWEBRTC)
    m_webNetworkMonitor.networkProcessCrashed();
#endif
}

void LibWebRTCNetwork::setConnection(RefPtr<IPC::Connection>&& connection)
{
#if USE(LIBWEBRTC)
    if (m_connection)
        m_connection->removeMessageReceiver(Messages::LibWebRTCNetwork::messageReceiverName());
#endif
    m_connection = WTFMove(connection);
#if USE(LIBWEBRTC)
    if (m_isActive)
        setSocketFactoryConnection();
    if (m_connection)
        m_connection->addMessageReceiver(*this, *this, Messages::LibWebRTCNetwork::messageReceiverName());
#endif
}

#if USE(LIBWEBRTC)
void LibWebRTCNetwork::setSocketFactoryConnection()
{
    if (!m_connection) {
        WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this]() mutable {
            m_socketFactory.setConnection(nullptr);
        });
        return;
    }
    m_connection->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::CreateRTCProvider(), [this, connection = m_connection]() mutable {
        if (!connection->isValid())
            return;

        WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this, connection = WTFMove(connection)]() mutable {
            m_socketFactory.setConnection(WTFMove(connection));
        });
    }, 0);
}
#endif

void LibWebRTCNetwork::dispatch(Function<void()>&& callback)
{
    if (!m_isActive) {
        RELEASE_LOG_ERROR(WebRTC, "Received WebRTCSocket message while libWebRTCNetwork is not active");
        return;
    }

#if USE(LIBWEBRTC)
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread(WTFMove(callback));
#else
    UNUSED_PARAM(callback);
#endif
}

#if USE(LIBWEBRTC)
void LibWebRTCNetwork::signalAddressReady(WebCore::LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& address)
{
    ASSERT(!WTF::isMainRunLoop());
    if (auto* socket = m_socketFactory.socket(identifier))
        socket->signalAddressReady(address.value);
}

void LibWebRTCNetwork::signalReadPacket(WebCore::LibWebRTCSocketIdentifier identifier, const IPC::DataReference& data, const RTCNetwork::IPAddress& address, uint16_t port, int64_t timestamp)
{
    ASSERT(!WTF::isMainRunLoop());
    if (auto* socket = m_socketFactory.socket(identifier))
        socket->signalReadPacket(data.data(), data.size(), rtc::SocketAddress(address.value, port), timestamp);
}

void LibWebRTCNetwork::signalSentPacket(WebCore::LibWebRTCSocketIdentifier identifier, int rtcPacketID, int64_t sendTimeMs)
{
    ASSERT(!WTF::isMainRunLoop());
    if (auto* socket = m_socketFactory.socket(identifier))
        socket->signalSentPacket(rtcPacketID, sendTimeMs);
}

void LibWebRTCNetwork::signalConnect(WebCore::LibWebRTCSocketIdentifier identifier)
{
    ASSERT(!WTF::isMainRunLoop());
    if (auto* socket = m_socketFactory.socket(identifier))
        socket->signalConnect();
}

void LibWebRTCNetwork::signalClose(WebCore::LibWebRTCSocketIdentifier identifier, int error)
{
    ASSERT(!WTF::isMainRunLoop());
    if (auto* socket = m_socketFactory.socket(identifier))
        socket->signalClose(error);
}

#endif

} // namespace WebKit
