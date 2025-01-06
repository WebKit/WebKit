/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "NetworkTransportSession.h"

#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkTransportStream.h"
#include "WebTransportSessionMessages.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkTransportSession);

#if !PLATFORM(COCOA)
void NetworkTransportSession::initialize(NetworkConnectionToWebProcess&, URL&&, WebKit::WebPageProxyIdentifier&&, WebCore::ClientOrigin&&, CompletionHandler<void(RefPtr<NetworkTransportSession>&&)>&& completionHandler)
{
    completionHandler(nullptr);
}
#endif

NetworkTransportSession::~NetworkTransportSession() = default;

IPC::Connection* NetworkTransportSession::messageSenderConnection() const
{
    return m_connectionToWebProcess ? &m_connectionToWebProcess->connection() : nullptr;
}

uint64_t NetworkTransportSession::messageSenderDestinationID() const
{
    return identifier().toUInt64();
}

#if !PLATFORM(COCOA)
void NetworkTransportSession::sendDatagram(std::span<const uint8_t>, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}
#endif

void NetworkTransportSession::sendStreamSendBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr stream = m_streams.get(identifier))
        stream->sendBytes(bytes, withFin);
    completionHandler();
}

void NetworkTransportSession::streamSendBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr stream = m_streams.get(identifier))
        stream->sendBytes(bytes, withFin);
    completionHandler();
}

#if !PLATFORM(COCOA)
void NetworkTransportSession::createOutgoingUnidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void NetworkTransportSession::createBidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
    completionHandler(std::nullopt);
}
#endif

void NetworkTransportSession::destroyOutgoingUnidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    ASSERT(m_streams.contains(identifier));
    m_streams.remove(identifier);
}

void NetworkTransportSession::destroyBidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    ASSERT(m_streams.contains(identifier));
    m_streams.remove(identifier);
}

void NetworkTransportSession::terminate(uint32_t, CString&&)
{
    // FIXME: Implement.
}

void NetworkTransportSession::receiveDatagram(std::span<const uint8_t> datagram)
{
    send(Messages::WebTransportSession::ReceiveDatagram(datagram));
}

void NetworkTransportSession::streamReceiveBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin)
{
    send(Messages::WebTransportSession::StreamReceiveBytes(identifier, bytes, withFin));
}

void NetworkTransportSession::receiveIncomingUnidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    send(Messages::WebTransportSession::ReceiveIncomingUnidirectionalStream(identifier));
}

void NetworkTransportSession::receiveBidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    send(Messages::WebTransportSession::ReceiveBidirectionalStream(identifier));
}

std::optional<SharedPreferencesForWebProcess> NetworkTransportSession::sharedPreferencesForWebProcess() const
{
    if (auto connectionToWebProcess = m_connectionToWebProcess.get())
        return connectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

}
