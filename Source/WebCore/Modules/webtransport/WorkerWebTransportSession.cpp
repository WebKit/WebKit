/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "WorkerWebTransportSession.h"

#include "ScriptExecutionContext.h"
#include "WebTransport.h"
#include "WebTransportBidirectionalStreamConstructionParameters.h"

namespace WebCore {

Ref<WorkerWebTransportSession> WorkerWebTransportSession::create(ScriptExecutionContextIdentifier contextID, WebTransportSessionClient& client, Ref<WebTransportSession>&& session)
{
    ASSERT(!RunLoop::isMain());
    return adoptRef(*new WorkerWebTransportSession(contextID, client, WTFMove(session)));
}

WorkerWebTransportSession::~WorkerWebTransportSession() = default;

WorkerWebTransportSession::WorkerWebTransportSession(ScriptExecutionContextIdentifier contextID, WebTransportSessionClient& client, Ref<WebTransportSession>&& session)
    : m_contextID(contextID)
    , m_client(client)
    , m_session(WTFMove(session))
{
    ASSERT(!RunLoop::isMain());
    m_session->attachClient(*this);
}

void WorkerWebTransportSession::receiveDatagram(std::span<const uint8_t> span)
{
    ASSERT(RunLoop::isMain());
    ScriptExecutionContext::postTaskTo(m_contextID, [vector = Vector<uint8_t> { span }, weakClient = m_client] (auto&) mutable {
        RefPtr client = weakClient.get();
        if (!client)
            return;
        client->receiveDatagram(vector.span());
    });
}

void WorkerWebTransportSession::networkProcessCrashed()
{
    ASSERT(RunLoop::isMain());
    ScriptExecutionContext::postTaskTo(m_contextID, [weakClient = m_client] (auto&) mutable {
        RefPtr client = weakClient.get();
        if (!client)
            return;
        client->networkProcessCrashed();
    });
}

void WorkerWebTransportSession::receiveIncomingUnidirectionalStream(WebTransportStreamIdentifier identifier)
{
    ASSERT(RunLoop::isMain());
    ScriptExecutionContext::postTaskTo(m_contextID, [identifier, weakClient = m_client] (auto&) mutable {
        RefPtr client = weakClient.get();
        if (!client)
            return;
        client->receiveIncomingUnidirectionalStream(identifier);
    });
}

void WorkerWebTransportSession::receiveBidirectionalStream(WebTransportBidirectionalStreamConstructionParameters&& parameters)
{
    ASSERT(RunLoop::isMain());
    ScriptExecutionContext::postTaskTo(m_contextID, [parameters = WTFMove(parameters), weakClient = m_client] (auto&) mutable {
        RefPtr client = weakClient.get();
        if (!client)
            return;
        client->receiveBidirectionalStream(WTFMove(parameters));
    });
}

void WorkerWebTransportSession::streamReceiveBytes(WebTransportStreamIdentifier identifier, std::span<const uint8_t> data, bool withFin)
{
    ASSERT(RunLoop::isMain());
    ScriptExecutionContext::postTaskTo(m_contextID, [identifier, data = Vector<uint8_t> { data }, withFin, weakClient = m_client] (auto&) mutable {
        RefPtr client = weakClient.get();
        if (!client)
            return;
        client->streamReceiveBytes(identifier, data.span(), withFin);
    });
}

Ref<GenericPromise> WorkerWebTransportSession::sendDatagram(std::span<const uint8_t> datagram)
{
    ASSERT(!RunLoop::isMain());
    return m_session->sendDatagram(datagram);
}

Ref<WritableStreamPromise> WorkerWebTransportSession::createOutgoingUnidirectionalStream()
{
    ASSERT(!RunLoop::isMain());
    return m_session->createOutgoingUnidirectionalStream();
}

Ref<BidirectionalStreamPromise> WorkerWebTransportSession::createBidirectionalStream()
{
    ASSERT(!RunLoop::isMain());
    return m_session->createBidirectionalStream();
}

void WorkerWebTransportSession::terminate(uint32_t code, CString&& reason)
{
    ASSERT(!RunLoop::isMain());
    m_session->terminate(code, WTFMove(reason));
}

}
