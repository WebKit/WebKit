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
#include <WebCore/Exception.h>
#include <WebCore/WebTransportConnectionInfo.h>
#include <WebCore/WebTransportConnectionStats.h>
#include <WebCore/WebTransportOptions.h>
#include <WebCore/WebTransportReceiveStreamStats.h>
#include <WebCore/WebTransportSendStreamSink.h>
#include <WebCore/WebTransportSendStreamStats.h>
#include <WebCore/WebTransportSessionClient.h>
#include <wtf/Ref.h>
#include <wtf/RunLoop.h>

namespace WebKit {

std::pair<Ref<WebTransportSession>, Ref<WebCore::WebTransportSessionPromise>> WebTransportSession::initialize(Ref<IPC::Connection>&& connection, ThreadSafeWeakPtr<WebCore::WebTransportSessionClient>&& client, const URL& url, const WebCore::WebTransportOptions& options, const WebPageProxyIdentifier& pageID, const WebCore::ClientOrigin& clientOrigin)
{
    auto identifier = WebTransportSessionIdentifier::generate();
    return {
        adoptRef(*new WebTransportSession(connection.copyRef(), WTFMove(client), identifier)),
        connection->sendWithPromisedReply(Messages::NetworkConnectionToWebProcess::InitializeWebTransportSession(identifier, url, options, pageID, clientOrigin))->whenSettled(RunLoop::mainSingleton(), [] (auto&& result) {
            if (result && *result)
                return WebCore::WebTransportSessionPromise::createAndResolve(WTFMove(**result));
            return WebCore::WebTransportSessionPromise::createAndReject();
        })
    };
}

WebTransportSession::WebTransportSession(Ref<IPC::Connection>&& connection, ThreadSafeWeakPtr<WebCore::WebTransportSessionClient>&& client, WebTransportSessionIdentifier identifier)
    : m_connection(WTFMove(connection))
    , m_client(WTFMove(client))
    , m_identifier(identifier)
{
    RELEASE_ASSERT(WebProcess::singleton().isWebTransportEnabled());
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
        strongClient->receiveDatagram(datagram, withFin, WTFMove(exception));
    else
        ASSERT_NOT_REACHED();
}

void WebTransportSession::receiveIncomingUnidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->receiveIncomingUnidirectionalStream(identifier);
    else
        ASSERT_NOT_REACHED();
}

void WebTransportSession::receiveBidirectionalStream(WebCore::WebTransportStreamIdentifier identifier)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->receiveBidirectionalStream(WebCore::WebTransportSendStreamSink::create(*this, identifier));
    else
        ASSERT_NOT_REACHED();
}

void WebTransportSession::streamReceiveBytes(WebCore::WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin, std::optional<WebCore::Exception>&& exception)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->streamReceiveBytes(identifier, bytes, withFin, WTFMove(exception));
    else
        ASSERT_NOT_REACHED();
}

void WebTransportSession::streamReceiveError(WebCore::WebTransportStreamIdentifier identifier, uint64_t errorCode)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->streamReceiveError(identifier, errorCode);
    else
        ASSERT_NOT_REACHED();
}

void WebTransportSession::streamSendError(WebCore::WebTransportStreamIdentifier identifier, uint64_t errorCode)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->streamSendError(identifier, errorCode);
    else
        ASSERT_NOT_REACHED();
}

void WebTransportSession::didFail(std::optional<uint32_t>&& code, String&& message)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->didFail(WTFMove(code), WTFMove(message));
    else
        ASSERT_NOT_REACHED();
}

void WebTransportSession::didDrain()
{
    ASSERT(RunLoop::isMain());
    if (RefPtr strongClient = m_client.get())
        strongClient->didDrain();
    else
        ASSERT_NOT_REACHED();
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

Ref<WebCore::WritableStreamPromise> WebTransportSession::createOutgoingUnidirectionalStream()
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::CreateOutgoingUnidirectionalStream())->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }] (auto&& identifier) mutable {
        ASSERT(RunLoop::isMain());
        RefPtr strongThis = weakThis.get();
        if (!identifier || !*identifier || !strongThis)
            return WebCore::WritableStreamPromise::createAndReject();
        return WebCore::WritableStreamPromise::createAndResolve(WebCore::WebTransportSendStreamSink::create(*strongThis, **identifier));
    });
}

Ref<WebCore::BidirectionalStreamPromise> WebTransportSession::createBidirectionalStream()
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::CreateBidirectionalStream())->whenSettled(RunLoop::mainSingleton(), [weakThis = ThreadSafeWeakPtr { *this }] (auto&& identifier) mutable {
        ASSERT(RunLoop::isMain());
        RefPtr strongThis = weakThis.get();
        if (!identifier || !*identifier || !strongThis)
            return WebCore::BidirectionalStreamPromise::createAndReject();
        return WebCore::BidirectionalStreamPromise::createAndResolve(WebCore::WebTransportSendStreamSink::create(*strongThis, **identifier));
    });
}

Ref<WebCore::WebTransportConnectionStatsPromise> WebTransportSession::getStats()
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::GetStats())->whenSettled(RunLoop::mainSingleton(), [] (auto&& stats) mutable {
        ASSERT(RunLoop::isMain());
        if (!stats)
            return WebCore::WebTransportConnectionStatsPromise::createAndReject();
        return WebCore::WebTransportConnectionStatsPromise::createAndResolve(WTFMove(*stats));
    });
}

Ref<WebCore::WebTransportSendStreamStatsPromise> WebTransportSession::getSendStreamStats(WebCore::WebTransportStreamIdentifier identifier)
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::GetSendStreamStats(identifier))->whenSettled(RunLoop::mainSingleton(), [] (auto&& stats) mutable {
        ASSERT(RunLoop::isMain());
        if (!stats || !*stats)
            return WebCore::WebTransportSendStreamStatsPromise::createAndReject();
        return WebCore::WebTransportSendStreamStatsPromise::createAndResolve(WTFMove(**stats));
    });
}

Ref<WebCore::WebTransportReceiveStreamStatsPromise> WebTransportSession::getReceiveStreamStats(WebCore::WebTransportStreamIdentifier identifier)
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::GetReceiveStreamStats(identifier))->whenSettled(RunLoop::mainSingleton(), [] (auto&& stats) mutable {
        ASSERT(RunLoop::isMain());
        if (!stats || !*stats)
            return WebCore::WebTransportReceiveStreamStatsPromise::createAndReject();
        return WebCore::WebTransportReceiveStreamStatsPromise::createAndResolve(WTFMove(**stats));
    });
}

Ref<WebCore::WebTransportSendStreamStatsPromise> WebTransportSession::getSendGroupStats(WebCore::WebTransportSendGroupIdentifier identifier)
{
    return sendWithPromisedReply(Messages::NetworkTransportSession::GetSendGroupStats(identifier))->whenSettled(RunLoop::mainSingleton(), [] (auto&& stats) mutable {
        ASSERT(RunLoop::isMain());
        if (!stats || !*stats)
            return WebCore::WebTransportSendStreamStatsPromise::createAndReject();
        return WebCore::WebTransportSendStreamStatsPromise::createAndResolve(WTFMove(**stats));
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
    send(Messages::NetworkTransportSession::Terminate(code, WTFMove(reason)));
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

void WebTransportSession::datagramIncomingHighWaterMarkUpdated(double watermark)
{
    send(Messages::NetworkTransportSession::DatagramIncomingHighWaterMarkUpdated(watermark));
}

void WebTransportSession::datagramOutgoingHighWaterMarkUpdated(double watermark)
{
    send(Messages::NetworkTransportSession::DatagramOutgoingHighWaterMarkUpdated(watermark));
}

}
