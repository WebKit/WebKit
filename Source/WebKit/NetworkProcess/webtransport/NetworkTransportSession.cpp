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
#include "WebCore/Exception.h"
#include "WebCore/ExceptionCode.h"
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
void NetworkTransportSession::sendDatagram(std::span<const uint8_t>, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}
#endif

void NetworkTransportSession::streamSendBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    if (RefPtr stream = m_streams.get(identifier))
        stream->sendBytes(bytes, withFin, WTFMove(completionHandler));
    else
        completionHandler(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError });
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

#if !PLATFORM(COCOA)
void NetworkTransportSession::terminate(WebCore::WebTransportSessionErrorCode, CString&&)
{
}
#endif

void NetworkTransportSession::receiveDatagram(std::span<const uint8_t> datagram, bool withFin, std::optional<WebCore::Exception>&& exception)
{
    send(Messages::WebTransportSession::ReceiveDatagram(datagram, withFin, WTFMove(exception)));
}

void NetworkTransportSession::streamReceiveBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin, std::optional<WebCore::Exception>&& exception)
{
    send(Messages::WebTransportSession::StreamReceiveBytes(identifier, bytes, withFin, WTFMove(exception)));
}

void NetworkTransportSession::receiveIncomingUnidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    send(Messages::WebTransportSession::ReceiveIncomingUnidirectionalStream(identifier));
}

void NetworkTransportSession::receiveBidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    send(Messages::WebTransportSession::ReceiveBidirectionalStream(identifier));
}

void NetworkTransportSession::cancelReceiveStream(WebCore::WebTransportStreamIdentifier identifier, std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    if (RefPtr stream = m_streams.get(identifier))
        stream->cancelReceive(errorCode);
    // Stream could have been destroyed gracefully when reads and writes were completed.
}

void NetworkTransportSession::cancelSendStream(WebCore::WebTransportStreamIdentifier identifier, std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    if (RefPtr stream = m_streams.get(identifier))
        stream->cancelSend(errorCode);
    // Stream could have been destroyed gracefully when reads and writes were completed.
}

void NetworkTransportSession::destroyStream(WebCore::WebTransportStreamIdentifier identifier, std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    if (RefPtr stream = m_streams.get(identifier)) {
        stream->cancel(errorCode);
        m_streams.remove(identifier);
    }
    // Stream could have been destroyed gracefully when reads and writes were completed.
}

std::optional<SharedPreferencesForWebProcess> NetworkTransportSession::sharedPreferencesForWebProcess() const
{
    if (auto connectionToWebProcess = m_connectionToWebProcess.get())
        return connectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

}
