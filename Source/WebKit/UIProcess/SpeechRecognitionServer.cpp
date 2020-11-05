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
#include "SpeechRecognitionServer.h"

#include "WebProcessProxy.h"
#include "WebSpeechRecognitionConnectionMessages.h"
#include <WebCore/SpeechRecognitionUpdate.h>

namespace WebKit {

SpeechRecognitionServer::SpeechRecognitionServer(Ref<IPC::Connection>&& connection, SpeechRecognitionServerIdentifier identifier)
    : m_connection(WTFMove(connection))
    , m_identifier(identifier)
{
}

void SpeechRecognitionServer::start(WebCore::SpeechRecognitionRequestInfo&& requestInfo)
{
    auto request = WebCore::SpeechRecognitionRequest::create(WTFMove(requestInfo));
    m_pendingRequests.append(WTFMove(request));

    processNextPendingRequestIfNeeded();
}

void SpeechRecognitionServer::processNextPendingRequestIfNeeded()
{
    if (m_currentRequest)
        return;

    if (m_pendingRequests.isEmpty())
        return;

    m_currentRequest = m_pendingRequests.takeFirst();
    startPocessingRequest(*m_currentRequest);
}

void SpeechRecognitionServer::stop(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    if (m_currentRequest && m_currentRequest->clientIdentifier() == clientIdentifier) {
        stopProcessingRequest(*m_currentRequest);
        return;
    }

    removePendingRequest(clientIdentifier);
}

void SpeechRecognitionServer::abort(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    if (m_currentRequest && m_currentRequest->clientIdentifier() == clientIdentifier) {
        m_currentRequest = nullptr;
        auto update = WebCore::SpeechRecognitionUpdate::create(clientIdentifier, WebCore::SpeechRecognitionUpdateType::End);
        send(Messages::WebSpeechRecognitionConnection::DidReceiveUpdate(update), m_identifier);

        processNextPendingRequestIfNeeded();
        return;
    }

    removePendingRequest(clientIdentifier);
}

void SpeechRecognitionServer::removePendingRequest(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    RefPtr<WebCore::SpeechRecognitionRequest> takenRequest;
    auto pendingRequests = std::exchange(m_pendingRequests, { });
    while (!pendingRequests.isEmpty()) {
        auto request = pendingRequests.takeFirst();
        if (request->clientIdentifier() == clientIdentifier) {
            auto update = WebCore::SpeechRecognitionUpdate::create(clientIdentifier, WebCore::SpeechRecognitionUpdateType::End);
            send(Messages::WebSpeechRecognitionConnection::DidReceiveUpdate(update), m_identifier);
            continue;
        }
        m_pendingRequests.append(WTFMove(request));
    }
}

void SpeechRecognitionServer::invalidate(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    if (m_currentRequest && m_currentRequest->clientIdentifier() == clientIdentifier) {
        m_currentRequest = nullptr;
        processNextPendingRequestIfNeeded();
    }
}

void SpeechRecognitionServer::startPocessingRequest(WebCore::SpeechRecognitionRequest&)
{
}

void SpeechRecognitionServer::stopProcessingRequest(WebCore::SpeechRecognitionRequest&)
{
}

IPC::Connection* SpeechRecognitionServer::messageSenderConnection() const
{
    return m_connection.ptr();
}

uint64_t SpeechRecognitionServer::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
}

} // namespace WebKit
