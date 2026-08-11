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
#include "WebTransportSession.h"

#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkTransportSessionMessages.h"
#include "WebProcess.h"
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/Document.h>
#include <WebCore/Exception.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/WebTransportConnectionInfo.h>
#include <WebCore/WebTransportConnectionStats.h>
#include <WebCore/WebTransportOptions.h>
#include <WebCore/WebTransportReceiveStreamStats.h>
#include <WebCore/WebTransportSendStreamStats.h>
#include <WebCore/WebTransportSessionClient.h>
#include <wtf/Ref.h>
#include <wtf/RunLoop.h>
#include <wtf/text/TextPosition.h>

namespace WebKit {

Ref<WebTransportSession> WebTransportSession::create(Ref<IPC::Connection>&& connection, ThreadSafeWeakPtr<WebCore::WebTransportSessionClient>&& client, const WebPageProxyIdentifier& pageID)
{
    return adoptRef(*new WebTransportSession(connection.copyRef(), WTF::move(client), WebTransportSessionIdentifier::generate(), pageID));
}

WebTransportSession::WebTransportSession(Ref<IPC::Connection>&& connection, ThreadSafeWeakPtr<WebCore::WebTransportSessionClient>&& client, WebTransportSessionIdentifier identifier, WebPageProxyIdentifier pageID)
    : m_connection(WTF::move(connection))
    , m_client(WTF::move(client))
    , m_identifier(identifier)
    , m_pageID(pageID)
{
    WebProcess::singleton().addWebTransportSession(m_identifier, *this);
}

WebTransportSession::~WebTransportSession()
{
    WebProcess::singleton().removeWebTransportSession(m_identifier);
    m_connection->send(Messages::NetworkConnectionToWebProcess::DestroyWebTransportSession(m_identifier), 0);
}

IPC::Connection* WebTransportSession::messageSenderConnection() const
{
    return m_connection.ptr();
}

uint64_t WebTransportSession::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
}

void WebTransportSession::receiveDatagram(std::span<const uint8_t> datagram, bool withFin, std::optional<WebCore::Exception>&& exception)
{
    ASSERT(RunLoop::isMain());
    if (auto strongClient = m_client.get())
        strongClient->receiveDatagram(datagram, withFin, WTF::move(exception));
}

void WebTransportSession::receiveIncomingUnidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->receiveIncomingUnidirectionalStream(identifier);
}

void WebTransportSession::receiveBidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->receiveBidirectionalStream(identifier);
}

void WebTransportSession::streamReceiveBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin, std::optional<WebCore::Exception>&& exception)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->streamReceiveBytes(identifier, bytes, withFin, WTF::move(exception));
}

void WebTransportSession::streamReceiveError(WebCore::WebTransportStreamIdentifier identifier, uint64_t errorCode)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->streamReceiveError(identifier, errorCode);
}

void WebTransportSession::streamSendError(WebCore::WebTransportStreamIdentifier identifier, uint64_t errorCode)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->streamSendError(identifier, errorCode);
}

void WebTransportSession::didFail(std::optional<uint32_t>&& code, String&& message)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->didFail(WTF::move(code), WTF::move(message));
}

void WebTransportSession::didDrain()
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->didDrain();
}

Ref<WebCore::WebTransportSessionInitializationPromise> WebTransportSession::initialize(WebCore::ScriptExecutionContext& context, const URL& url, const WebCore::WebTransportOptions& options, const WebCore::ClientOrigin& origin)
{
    std::optional<TextPosition> sourcePosition;
    if (RefPtr document = dynamicDowncast<WebCore::Document>(context))
        sourcePosition = document->currentParserSourcePosition();
    if (CheckedPtr csp = context.contentSecurityPolicy(); !csp || !csp->allowConnectToSource(url, WTF::move(sourcePosition)))
        return WebCore::WebTransportSessionInitializationPromise::createAndReject();
    return sendWithPromisedReply(Messages::NetworkConnectionToWebProcess::InitializeWebTransportSession(m_identifier, url, options, m_pageID, origin))->whenSettled(RunLoop::mainSingleton(), [] (auto&& result) {
        if (result && *result)
            return WebCore::WebTransportSessionInitializationPromise::createAndResolve(WTF::move(**result));
        return WebCore::WebTransportSessionInitializationPromise::createAndReject();
    });
}

Ref<WebCore::WebTransportSendPromise> WebTransportSession::sendDatagram(std::optional<WebCore::WebTransportSendGroupIdentifier> identifier, std::span<const uint8_t> datagram)
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::SendDatagram(identifier, datagram))->whenSettled(RunLoop::mainSingleton(), [] (auto&& exception) {
        ASSERT(RunLoop::isMain());
        if (!exception)
            return WebCore::WebTransportSendPromise::createAndReject();
        return WebCore::WebTransportSendPromise::createAndResolve(*exception);
    });
}

Ref<WebCore::WebTransportStreamPromise> WebTransportSession::createOutgoingUnidirectionalStream()
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::CreateOutgoingUnidirectionalStream())->whenSettled(RunLoop::mainSingleton(), [] (auto&& identifier) mutable {
        ASSERT(RunLoop::isMain());
        if (!identifier || !*identifier)
            return WebCore::WebTransportStreamPromise::createAndReject();
        return WebCore::WebTransportStreamPromise::createAndResolve(**identifier);
    });
}

Ref<WebCore::WebTransportStreamPromise> WebTransportSession::createBidirectionalStream()
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::CreateBidirectionalStream())->whenSettled(RunLoop::mainSingleton(), [] (auto&& identifier) mutable {
        ASSERT(RunLoop::isMain());
        if (!identifier || !*identifier)
            return WebCore::WebTransportStreamPromise::createAndReject();
        return WebCore::WebTransportStreamPromise::createAndResolve(**identifier);
    });
}

Ref<WebCore::WebTransportConnectionStatsPromise> WebTransportSession::getStats()
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::GetStats())->whenSettled(RunLoop::mainSingleton(), [] (auto&& stats) mutable {
        ASSERT(RunLoop::isMain());
        if (!stats || !*stats)
            return WebCore::WebTransportConnectionStatsPromise::createAndReject();
        return WebCore::WebTransportConnectionStatsPromise::createAndResolve(WTF::move(**stats));
    });
}

Ref<WebCore::WebTransportSendStreamStatsPromise> WebTransportSession::getSendStreamStats(WebCore::WebTransportStreamIdentifier identifier)
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::GetSendStreamStats(identifier))->whenSettled(RunLoop::mainSingleton(), [] (auto&& stats) mutable {
        ASSERT(RunLoop::isMain());
        if (!stats || !*stats)
            return WebCore::WebTransportSendStreamStatsPromise::createAndReject();
        return WebCore::WebTransportSendStreamStatsPromise::createAndResolve(WTF::move(**stats));
    });
}

Ref<WebCore::WebTransportReceiveStreamStatsPromise> WebTransportSession::getReceiveStreamStats(WebCore::WebTransportStreamIdentifier identifier)
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::GetReceiveStreamStats(identifier))->whenSettled(RunLoop::mainSingleton(), [] (auto&& stats) mutable {
        ASSERT(RunLoop::isMain());
        if (!stats || !*stats)
            return WebCore::WebTransportReceiveStreamStatsPromise::createAndReject();
        return WebCore::WebTransportReceiveStreamStatsPromise::createAndResolve(WTF::move(**stats));
    });
}

Ref<WebCore::WebTransportSendStreamStatsPromise> WebTransportSession::getSendGroupStats(WebCore::WebTransportSendGroupIdentifier identifier)
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::GetSendGroupStats(identifier))->whenSettled(RunLoop::mainSingleton(), [] (auto&& stats) mutable {
        ASSERT(RunLoop::isMain());
        if (!stats || !*stats)
            return WebCore::WebTransportSendStreamStatsPromise::createAndReject();
        return WebCore::WebTransportSendStreamStatsPromise::createAndResolve(WTF::move(**stats));
    });
}

Ref<WebCore::WebTransportExportKeyingMaterialPromise> WebTransportSession::exportKeyingMaterial(std::span<const uint8_t> label, std::span<const uint8_t> context, uint32_t outputLength)
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::ExportKeyingMaterial(label, context, outputLength))->whenSettled(RunLoop::mainSingleton(), [] (auto&& keyingMaterial) mutable {
        ASSERT(RunLoop::isMain());
        if (!keyingMaterial || !*keyingMaterial)
            return WebCore::WebTransportExportKeyingMaterialPromise::createAndReject();
        return WebCore::WebTransportExportKeyingMaterialPromise::createAndResolve(WTF::move(**keyingMaterial));
    });
}

Ref<WebCore::WebTransportSendPromise> WebTransportSession::streamSendBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin)
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::StreamSendBytes(identifier, bytes, withFin))->whenSettled(RunLoop::mainSingleton(), [] (auto&& exception) {
        if (!exception)
            return WebCore::WebTransportSendPromise::createAndReject();
        return WebCore::WebTransportSendPromise::createAndResolve(*exception);
    });
}

void WebTransportSession::terminate(WebCore::WebTransportSessionErrorCode code, CString&& reason)
{
    send(Messages::NetworkTransportSession::Terminate(code, WTF::move(reason)));
}

void WebTransportSession::cancelReceiveStream(WebCore::WebTransportStreamIdentifier identifier, std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    send(Messages::NetworkTransportSession::CancelReceiveStream(identifier, errorCode));
}

void WebTransportSession::cancelSendStream(WebCore::WebTransportStreamIdentifier identifier, std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    send(Messages::NetworkTransportSession::CancelSendStream(identifier, errorCode));
}

void WebTransportSession::destroyStream(WebCore::WebTransportStreamIdentifier identifier, std::optional<WebCore::WebTransportStreamErrorCode> errorCode)
{
    send(Messages::NetworkTransportSession::DestroyStream(identifier, errorCode));
}

void WebTransportSession::datagramIncomingMaxAgeUpdated(std::optional<double> maxAge)
{
    send(Messages::NetworkTransportSession::DatagramIncomingMaxAgeUpdated(maxAge));
}

void WebTransportSession::datagramOutgoingMaxAgeUpdated(std::optional<double> maxAge)
{
    send(Messages::NetworkTransportSession::DatagramOutgoingMaxAgeUpdated(maxAge));
}

void WebTransportSession::incomingMaxBufferedDatagramsUpdated(uint32_t value)
{
    send(Messages::NetworkTransportSession::IncomingMaxBufferedDatagramsUpdated(value));
}

void WebTransportSession::outgoingMaxBufferedDatagramsUpdated(uint32_t value)
{
    send(Messages::NetworkTransportSession::OutgoingMaxBufferedDatagramsUpdated(value));
}

}
