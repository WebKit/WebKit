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

#include "SpeechRecognitionPermissionRequest.h"
#include "WebProcessProxy.h"
#include "WebSpeechRecognitionConnectionMessages.h"
#include <WebCore/SpeechRecognitionRequestInfo.h>
#include <WebCore/SpeechRecognitionUpdate.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, messageSenderConnection())

namespace WebKit {

SpeechRecognitionServer::SpeechRecognitionServer(Ref<IPC::Connection>&& connection, SpeechRecognitionServerIdentifier identifier, SpeechRecognitionPermissionChecker&& permissionChecker)
    : m_connection(WTFMove(connection))
    , m_identifier(identifier)
    , m_permissionChecker(WTFMove(permissionChecker))
{
}

void SpeechRecognitionServer::start(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier, String&& lang, bool continuous, bool interimResults, uint64_t maxAlternatives, WebCore::ClientOrigin&& origin)
{
    MESSAGE_CHECK(clientIdentifier);
    ASSERT(!m_pendingRequests.contains(clientIdentifier));
    ASSERT(!m_ongoingRequests.contains(clientIdentifier));
    auto requestInfo = WebCore::SpeechRecognitionRequestInfo { clientIdentifier, WTFMove(lang), continuous, interimResults, maxAlternatives, WTFMove(origin) };
    auto& pendingRequest = m_pendingRequests.add(clientIdentifier, makeUnique<WebCore::SpeechRecognitionRequest>(WTFMove(requestInfo))).iterator->value;

    requestPermissionForRequest(*pendingRequest);
}

void SpeechRecognitionServer::requestPermissionForRequest(WebCore::SpeechRecognitionRequest& request)
{
    m_permissionChecker(request.clientOrigin(), [this, weakThis = makeWeakPtr(this), weakRequest = makeWeakPtr(request)](SpeechRecognitionPermissionDecision decision) mutable {
        if (!weakThis)
            return;

        if (!weakRequest)
            return;

        auto identifier = weakRequest->clientIdentifier();
        auto takenRequest = m_pendingRequests.take(identifier);
        if (decision == SpeechRecognitionPermissionDecision::Deny) {
            auto error = WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::NotAllowed, "Permission check failed"_s };
            sendUpdate(identifier, WebCore::SpeechRecognitionUpdateType::Error, error);
            return;
        }

        m_ongoingRequests.add(identifier, WTFMove(takenRequest));
        handleRequest(*m_ongoingRequests.get(identifier));
    });
}

void SpeechRecognitionServer::stop(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    MESSAGE_CHECK(clientIdentifier);
    if (m_pendingRequests.remove(clientIdentifier)) {
        sendUpdate(clientIdentifier, WebCore::SpeechRecognitionUpdateType::End);
        return;
    }

    ASSERT(m_ongoingRequests.contains(clientIdentifier));
    stopRequest(*m_ongoingRequests.get(clientIdentifier));
}

void SpeechRecognitionServer::abort(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    MESSAGE_CHECK(clientIdentifier);
    if (m_pendingRequests.remove(clientIdentifier)) {
        sendUpdate(clientIdentifier, WebCore::SpeechRecognitionUpdateType::End);
        return;
    }

    ASSERT(m_ongoingRequests.contains(clientIdentifier));
    auto request = m_ongoingRequests.take(clientIdentifier);
    abortRequest(*request);
    auto update = WebCore::SpeechRecognitionUpdate::create(clientIdentifier, WebCore::SpeechRecognitionUpdateType::End);
    send(Messages::WebSpeechRecognitionConnection::DidReceiveUpdate(update), m_identifier);
}

void SpeechRecognitionServer::invalidate(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    MESSAGE_CHECK(clientIdentifier);
    auto request = m_ongoingRequests.take(clientIdentifier);
    if (request)
        abortRequest(*request);
}

void SpeechRecognitionServer::handleRequest(WebCore::SpeechRecognitionRequest& request)
{
    // TODO: start capturing audio and recognition.
}

void SpeechRecognitionServer::stopRequest(WebCore::SpeechRecognitionRequest& request)
{
    // TODO: stop capturing audio and finalizing results by recognizing captured audio.
}

void SpeechRecognitionServer::abortRequest(WebCore::SpeechRecognitionRequest& request)
{
    // TODO: stop capturing audio and recognition immediately without generating results.
}

void SpeechRecognitionServer::sendUpdate(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier, WebCore::SpeechRecognitionUpdateType type, Optional<WebCore::SpeechRecognitionError> error, Optional<Vector<WebCore::SpeechRecognitionResultData>> result)
{
    auto update = WebCore::SpeechRecognitionUpdate::create(clientIdentifier, type);
    if (type == WebCore::SpeechRecognitionUpdateType::Error)
        update = WebCore::SpeechRecognitionUpdate::createError(clientIdentifier, *error);
    if (type == WebCore::SpeechRecognitionUpdateType::Result)
        update = WebCore::SpeechRecognitionUpdate::createResult(clientIdentifier, *result);
    send(Messages::WebSpeechRecognitionConnection::DidReceiveUpdate(update), m_identifier);
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

#undef MESSAGE_CHECK
