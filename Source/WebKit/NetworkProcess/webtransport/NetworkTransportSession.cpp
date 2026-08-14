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
#include <WebCore/Exception.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/WebTransportConnectionStats.h>
#include <WebCore/WebTransportReceiveStreamStats.h>
#include <WebCore/WebTransportSendStreamStats.h>
#include <wtf/ReducedResolutionSeconds.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkTransportSession);

NetworkTransportSession::~NetworkTransportSession()
{
    for (auto&& statsRequest : std::exchange(m_statsRequestsBeforeInitialization, { }))
        statsRequest(std::nullopt);
    for (auto&& streamRequest : std::exchange(m_streamRequestsBeforeInitialization, { }))
        streamRequest.completionHandler(std::nullopt);
}

IPC::Connection* NetworkTransportSession::messageSenderConnection() const
{
    return m_connectionToWebProcess ? &m_connectionToWebProcess->connection() : nullptr;
}

uint64_t NetworkTransportSession::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
}

void NetworkTransportSession::streamSendBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    if (RefPtr stream = m_streams.get(identifier))
        stream->sendBytes(bytes, withFin, WTF::move(completionHandler));
    else
        completionHandler(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError });
}

void NetworkTransportSession::receiveDatagram(std::span<const uint8_t> datagram, bool withFin, std::optional<WebCore::Exception>&& exception)
{
    m_datagramBytesReceived += datagram.size();
    send(Messages::WebTransportSession::ReceiveDatagram(datagram, withFin, WTF::move(exception)));
}

void NetworkTransportSession::streamReceiveBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin, std::optional<WebCore::Exception>&& exception)
{
    send(Messages::WebTransportSession::StreamReceiveBytes(identifier, bytes, withFin, WTF::move(exception)));
}

void NetworkTransportSession::streamReceiveError(WebCore::WebTransportStreamIdentifier identifier, uint64_t errorCode)
{
    send(Messages::WebTransportSession::StreamReceiveError(identifier, errorCode));
}

void NetworkTransportSession::streamSendError(WebCore::WebTransportStreamIdentifier identifier, uint64_t errorCode)
{
    send(Messages::WebTransportSession::StreamSendError(identifier, errorCode));
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
}

void NetworkTransportSession::cancelSendStream(WebCore::WebTransportStreamIdentifier identifier, std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    if (RefPtr stream = m_streams.get(identifier))
        stream->cancelSend(errorCode);
}

void NetworkTransportSession::destroyStream(WebCore::WebTransportStreamIdentifier identifier, std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    if (RefPtr stream = m_streams.take(identifier)) {
        stream->cancel(errorCode);
        m_bytesSentOnClosedStreams += stream->bytesSent();
        m_bytesReceivedOnClosedStreams += stream->bytesReceived();
    }
}

std::optional<SharedPreferencesForWebProcess> NetworkTransportSession::sharedPreferencesForWebProcess() const
{
    if (auto connectionToWebProcess = m_connectionToWebProcess.get())
        return connectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

void NetworkTransportSession::getSendStreamStats(WebCore::WebTransportStreamIdentifier identifier, CompletionHandler<void(std::optional<WebCore::WebTransportSendStreamStats>&&)>&& completionHandler)
{
    if (RefPtr stream = m_streams.get(identifier))
        completionHandler(stream->getSendStreamStats());
    else
        completionHandler(std::nullopt);
}

void NetworkTransportSession::getReceiveStreamStats(WebCore::WebTransportStreamIdentifier identifier, CompletionHandler<void(std::optional<WebCore::WebTransportReceiveStreamStats>&&)>&& completionHandler)
{
    if (RefPtr stream = m_streams.get(identifier))
        completionHandler(stream->getReceiveStreamStats());
    else
        completionHandler(std::nullopt);
}

void NetworkTransportSession::getSendGroupStats(WebCore::WebTransportSendGroupIdentifier identifier, CompletionHandler<void(std::optional<WebCore::WebTransportSendStreamStats>&&)>&& completionHandler)
{
    // FIXME: Get better data from the stream.
    uint64_t bytesSent = m_datagramBytesSent.get(identifier);
    completionHandler(WebCore::WebTransportSendStreamStats {
        bytesSent,
        bytesSent,
        bytesSent
    });
}

void NetworkTransportSession::datagramIncomingMaxAgeUpdated(std::optional<double>)
{
    // FIXME: Use this value.
}

void NetworkTransportSession::datagramOutgoingMaxAgeUpdated(std::optional<double>)
{
    // FIXME: Use this value.
}

void NetworkTransportSession::incomingMaxBufferedDatagramsUpdated(uint32_t)
{
    // FIXME: Use this value.
}

void NetworkTransportSession::outgoingMaxBufferedDatagramsUpdated(uint32_t)
{
    // FIXME: Use this value.
}

void NetworkTransportSession::getStats(CompletionHandler<void(std::optional<WebCore::WebTransportConnectionStats>&&)>&& completionHandler)
{
    switch (m_initializationState) {
    case InitializationState::Waiting:
        m_statsRequestsBeforeInitialization.append(WTF::move(completionHandler));
        return;
    case InitializationState::Failed:
        return completionHandler(std::nullopt);
    case InitializationState::Succeeded:
        break;
    }

    uint64_t bytesSent = m_bytesSentOnClosedStreams;
    uint64_t bytesReceived = m_bytesReceivedOnClosedStreams;
    for (Ref stream : m_streams.values()) {
        bytesSent += stream->bytesSent();
        bytesReceived += stream->bytesReceived();
    }
    for (uint64_t datagramBytesSent : m_datagramBytesSent.values())
        bytesSent += datagramBytesSent;
    bytesReceived += m_datagramBytesReceived;

    completionHandler(WebCore::WebTransportConnectionStats {
        bytesSent,
        bytesReceived
    });
}

void NetworkTransportSession::completeRequestsAfterInitialization(std::optional<Seconds> initializationTime)
{
    ASSERT(m_initializationState == InitializationState::Waiting);
    if (!initializationTime) {
        m_initializationState = InitializationState::Failed;
        for (auto&& statsRequest : std::exchange(m_statsRequestsBeforeInitialization, { }))
            statsRequest(std::nullopt);
        for (auto&& streamRequest : std::exchange(m_streamRequestsBeforeInitialization, { }))
            streamRequest.completionHandler(std::nullopt);
        return;
    }
    m_initializationState = InitializationState::Succeeded;
    for (auto&& statsRequest : std::exchange(m_statsRequestsBeforeInitialization, { }))
        getStats(WTF::move(statsRequest));
    for (auto&& streamRequest : std::exchange(m_streamRequestsBeforeInitialization, { }))
        createStream(streamRequest.streamType, WTF::move(streamRequest.completionHandler));
}

#if !HAVE(WEBTRANSPORT)
RefPtr<NetworkTransportSession> NetworkTransportSession::create(NetworkConnectionToWebProcess&, WebTransportSessionIdentifier, URL&&, WebCore::WebTransportOptions&&, Vector<KeyValuePair<String, String>>&&, WebKit::WebPageProxyIdentifier&&, WebCore::ClientOrigin&&)
{
    return nullptr;
}

void NetworkTransportSession::initialize(CompletionHandler<void(std::optional<WebCore::WebTransportConnectionInfo>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

NetworkTransportSession::NetworkTransportSession()
    : m_identifier(WebTransportSessionIdentifier::generate())
{
}

void NetworkTransportSession::sendDatagram(std::optional<WebCore::WebTransportSendGroupIdentifier>, std::span<const uint8_t>, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void NetworkTransportSession::createOutgoingUnidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void NetworkTransportSession::createBidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void NetworkTransportSession::createStream(NetworkTransportStreamType, CompletionHandler<void(std::optional<WebCore::WebTransportStreamIdentifier>)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void NetworkTransportSession::terminate(WebCore::WebTransportSessionErrorCode, CString&&)
{
}

bool NetworkTransportSession::isSessionClosed() const
{
    return false;
}

void NetworkTransportSession::exportKeyingMaterial(std::span<const uint8_t>, std::span<const uint8_t>, uint32_t, CompletionHandler<void(std::optional<Vector<uint8_t>>)>&& completionHandler)
{
    completionHandler(std::nullopt);
}
#endif

}
