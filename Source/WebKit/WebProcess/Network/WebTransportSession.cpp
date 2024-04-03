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
#include "WebTransportReceiveStreamSource.h"
#include "WebTransportSendStreamSink.h"
#include <WebCore/WebTransportBidirectionalStreamConstructionParameters.h>
#include <WebCore/WebTransportSessionClient.h>
#include <wtf/Ref.h>
#include <wtf/RunLoop.h>

namespace WebKit {

void WebTransportSession::initialize(const URL& url, CompletionHandler<void(RefPtr<WebTransportSession>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::InitializeWebTransportSession(url), [completionHandler = WTFMove(completionHandler)] (std::optional<WebTransportSessionIdentifier> identifier) mutable {
        ASSERT(RunLoop::isMain());
        if (!identifier)
            return completionHandler(nullptr);
        completionHandler(adoptRef(new WebTransportSession(*identifier)));
    });
}

WebTransportSession::WebTransportSession(WebTransportSessionIdentifier identifier)
    : m_identifier(identifier)
{
    ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(WebProcess::singleton().isWebTransportEnabled());
    WebProcess::singleton().addWebTransportSession(m_identifier, *this);
}

WebTransportSession::~WebTransportSession()
{
    ASSERT(RunLoop::isMain());
    WebProcess::singleton().removeWebTransportSession(m_identifier);
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::DestroyWebTransportSession(m_identifier), 0);
}

IPC::Connection* WebTransportSession::messageSenderConnection() const
{
    // FIXME: If the network process crashes, we should not use this same identifier with a new network process.
    // We should instead make this session fail.
    ASSERT(RunLoop::isMain());
    return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

uint64_t WebTransportSession::messageSenderDestinationID() const
{
    ASSERT(RunLoop::isMain());
    return m_identifier.toUInt64();
}

void WebTransportSession::receiveDatagram(std::span<const uint8_t> datagram)
{
    ASSERT(RunLoop::isMain());
    if (auto strongClient = m_client.get())
        strongClient->receiveDatagram(datagram);
}

void WebTransportSession::receiveIncomingUnidirectionalStream(WebTransportStreamIdentifier)
{
    ASSERT(RunLoop::isMain());
    if (auto strongClient = m_client.get())
        strongClient->receiveIncomingUnidirectionalStream();
}

void WebTransportSession::receiveBidirectionalStream(WebTransportStreamIdentifier)
{
    ASSERT(RunLoop::isMain());
    if (auto strongClient = m_client.get())
        strongClient->receiveBidirectionalStream();
}

void WebTransportSession::streamReceiveBytes(WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin)
{
    ASSERT(RunLoop::isMain());
    if (auto source = m_readStreamSources.get(identifier))
        source->receiveBytes(bytes, withFin);
}

void WebTransportSession::sendDatagram(std::span<const uint8_t> datagram, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    sendWithAsyncReply(Messages::NetworkTransportSession::SendDatagram(datagram), WTFMove(completionHandler));
}

void WebTransportSession::createOutgoingUnidirectionalStream(CompletionHandler<void(RefPtr<WebCore::WritableStreamSink>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    sendWithAsyncReply(Messages::NetworkTransportSession::CreateOutgoingUnidirectionalStream(), [this, weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (std::optional<WebTransportStreamIdentifier> identifier) mutable {
        if (!identifier || !weakThis)
            return completionHandler(nullptr);
        completionHandler(WebTransportSendStreamSink::create(*this, *identifier));
    });
}

void WebTransportSession::createBidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportBidirectionalStreamConstructionParameters>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    sendWithAsyncReply(Messages::NetworkTransportSession::CreateBidirectionalStream(), [this, weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (std::optional<WebTransportStreamIdentifier> identifier) mutable {
        if (!identifier || !weakThis)
            return completionHandler(std::nullopt);

        auto readStreamSource = WebTransportReceiveStreamSource::create();
        ASSERT(!m_readStreamSources.get(*identifier));
        m_readStreamSources.set(*identifier, readStreamSource);

        WebCore::WebTransportBidirectionalStreamConstructionParameters parameters {
            WTFMove(readStreamSource),
            WebTransportSendStreamSink::create(*this, *identifier)
        };
        completionHandler(WTFMove(parameters));
    });
}

void WebTransportSession::streamSendBytes(WebTransportStreamIdentifier identifier, std::span<const uint8_t> bytes, bool withFin, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkTransportSession::StreamSendBytes(identifier, bytes, withFin), WTFMove(completionHandler));
}

void WebTransportSession::terminate(uint32_t code, CString&& reason)
{
    ASSERT(RunLoop::isMain());
    send(Messages::NetworkTransportSession::Terminate(code, WTFMove(reason)));
}

}
