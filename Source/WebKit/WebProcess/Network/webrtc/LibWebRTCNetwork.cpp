/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC)

#include "LibWebRTCNetworkMessages.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "WebProcess.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCNetwork);

LibWebRTCNetwork::LibWebRTCNetwork(WebProcess& webProcess)
    : WebRTCNetworkBase(webProcess)
    , m_webNetworkMonitor(*this)
{
}

LibWebRTCNetwork::~LibWebRTCNetwork()
{
    ASSERT_NOT_REACHED();
}

void LibWebRTCNetwork::setAsActive()
{
    WebRTCNetworkBase::setAsActive();
    if (m_connection)
        setSocketFactoryConnection();
}

void LibWebRTCNetwork::networkProcessCrashed()
{
    setConnection(nullptr);

    protect(monitor())->networkProcessCrashed();
}

void LibWebRTCNetwork::setConnection(RefPtr<IPC::Connection>&& connection)
{
    if (RefPtr connection = m_connection)
        connection->removeMessageReceiver(Messages::LibWebRTCNetwork::messageReceiverName());

    m_connection = WTF::move(connection);

    if (isActive())
        setSocketFactoryConnection();
    if (RefPtr connection = m_connection)
        connection->addMessageReceiver(*this, *this, Messages::LibWebRTCNetwork::messageReceiverName());
}

void LibWebRTCNetwork::setSocketFactoryConnection()
{
    RefPtr connection = m_connection;
    if (!connection) {
        WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this, protectedThis = Ref { *this }]() mutable {
            m_socketFactory.setConnection(nullptr);
        });
        return;
    }
    connection->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::CreateRTCProvider(), [this, protectedThis = Ref { *this }, connection]() mutable {
        if (!connection->isValid())
            return;

        WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([this, protectedThis = Ref { *this }, connection = WTF::move(connection)]() mutable {
            m_socketFactory.setConnection(WTF::move(connection));
        });
    }, 0);
}

void LibWebRTCNetwork::dispatch(Function<void()>&& callback)
{
    if (!isActive()) {
        RELEASE_LOG_ERROR(WebRTC, "Received WebRTCSocket message while libWebRTCNetwork is not active");
        return;
    }

    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread(WTF::move(callback));
}

static webrtc::EcnMarking convertToWebRTCEcnMarking(WebRTCNetwork::EcnMarking ecn)
{
    switch (ecn) {
    case WebRTCNetwork::EcnMarking::kNotEct:
        return webrtc::EcnMarking::kNotEct;
    case WebRTCNetwork::EcnMarking::kEct1:
        return webrtc::EcnMarking::kEct1;
    case WebRTCNetwork::EcnMarking::kEct0:
        return webrtc::EcnMarking::kEct0;
    case WebRTCNetwork::EcnMarking::kCe:
        return webrtc::EcnMarking::kCe;
    }

    ASSERT_NOT_REACHED();
    return webrtc::EcnMarking::kNotEct;
}

void LibWebRTCNetwork::signalAddressReady(WebCore::LibWebRTCSocketIdentifier identifier, const RTCNetwork::SocketAddress& address)
{
    ASSERT(!WTF::isMainRunLoop());
    if (auto* socket = m_socketFactory.socket(identifier))
        socket->signalAddressReady(address.rtcAddress());
}

void LibWebRTCNetwork::signalReadPacket(WebCore::LibWebRTCSocketIdentifier identifier, std::span<const uint8_t> data, const RTCNetwork::IPAddress& address, uint16_t port, int64_t timestamp, WebRTCNetwork::EcnMarking ecn)
{
    ASSERT(!WTF::isMainRunLoop());
    if (auto* socket = m_socketFactory.socket(identifier))
        socket->signalReadPacket(data, webrtc::SocketAddress(address.rtcAddress(), port), timestamp, convertToWebRTCEcnMarking(ecn));
}

void LibWebRTCNetwork::signalSentPacket(WebCore::LibWebRTCSocketIdentifier identifier, int64_t rtcPacketID, int64_t sendTimeMs)
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

void LibWebRTCNetwork::signalUsedInterface(WebCore::LibWebRTCSocketIdentifier identifier, String&& interfaceName)
{
    ASSERT(!WTF::isMainRunLoop());
    if (auto* socket = m_socketFactory.socket(identifier))
        socket->signalUsedInterface(WTF::move(interfaceName));
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
