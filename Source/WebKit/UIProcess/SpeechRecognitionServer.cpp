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

#include "UserMediaProcessManager.h"
#include "WebProcessProxy.h"
#include "WebSpeechRecognitionConnectionMessages.h"
#include <WebCore/SpeechRecognitionRequestInfo.h>
#include <WebCore/SpeechRecognitionUpdate.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, messageSenderConnection())

namespace WebKit {

SpeechRecognitionServer::SpeechRecognitionServer(Ref<IPC::Connection>&& connection, SpeechRecognitionServerIdentifier identifier, SpeechRecognitionPermissionChecker&& permissionChecker, SpeechRecognitionCheckIfMockSpeechRecognitionEnabled&& checkIfEnabled
#if ENABLE(MEDIA_STREAM)
    , RealtimeMediaSourceCreateFunction&& function
#endif
    )
    : m_connection(WTFMove(connection))
    , m_identifier(identifier)
    , m_permissionChecker(WTFMove(permissionChecker))
    , m_checkIfMockSpeechRecognitionEnabled(WTFMove(checkIfEnabled))
#if ENABLE(MEDIA_STREAM)
    , m_realtimeMediaSourceCreateFunction(WTFMove(function))
#endif
{
}

void SpeechRecognitionServer::start(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier, String&& lang, bool continuous, bool interimResults, uint64_t maxAlternatives, WebCore::ClientOrigin&& origin, WebCore::FrameIdentifier frameIdentifier)
{
    MESSAGE_CHECK(clientIdentifier);
    ASSERT(!m_requests.contains(clientIdentifier));
    auto requestInfo = WebCore::SpeechRecognitionRequestInfo { clientIdentifier, WTFMove(lang), continuous, interimResults, maxAlternatives, WTFMove(origin), frameIdentifier };
    auto& newRequest = m_requests.add(clientIdentifier, makeUnique<WebCore::SpeechRecognitionRequest>(WTFMove(requestInfo))).iterator->value;

    requestPermissionForRequest(*newRequest);
}

void SpeechRecognitionServer::requestPermissionForRequest(WebCore::SpeechRecognitionRequest& request)
{
    m_permissionChecker(request, [this, weakThis = makeWeakPtr(this), weakRequest = makeWeakPtr(request)](auto error) mutable {
        if (!weakThis || !weakRequest)
            return;

        auto identifier = weakRequest->clientIdentifier();
        auto request = m_requests.take(identifier);
        if (error) {
            sendUpdate(identifier, WebCore::SpeechRecognitionUpdateType::Error, WTFMove(error));
            return;
        }

        ASSERT(request);
        handleRequest(makeUniqueRefFromNonNullUniquePtr(WTFMove(request)));
    });
}

void SpeechRecognitionServer::handleRequest(UniqueRef<WebCore::SpeechRecognitionRequest>&& request)
{
    if (m_recognizer) {
        m_recognizer->abort(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::Aborted, "Another request is started"_s });
        m_recognizer->prepareForDestruction();
    }

    auto clientIdentifier = request->clientIdentifier();
    m_recognizer = makeUnique<WebCore::SpeechRecognizer>([this, weakThis = makeWeakPtr(this)](auto& update) {
        if (!weakThis)
            return;

        sendUpdate(update);

        if (update.type() == WebCore::SpeechRecognitionUpdateType::Error)
            m_recognizer->abort();
        else if (update.type() == WebCore::SpeechRecognitionUpdateType::End)
            m_recognizer->setInactive();
    }, WTFMove(request));

#if ENABLE(MEDIA_STREAM)
    auto sourceOrError = m_realtimeMediaSourceCreateFunction();
    if (!sourceOrError) {
        sendUpdate(WebCore::SpeechRecognitionUpdate::createError(clientIdentifier, WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::AudioCapture, sourceOrError.errorMessage }));
        return;
    }

    WebProcessProxy::muteCaptureInPagesExcept(m_identifier);
    bool mockDeviceCapturesEnabled = m_checkIfMockSpeechRecognitionEnabled();
    m_recognizer->start(sourceOrError.source(), mockDeviceCapturesEnabled);
#else
    sendUpdate(clientIdentifier, WebCore::SpeechRecognitionUpdateType::Error, WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::AudioCapture, "Audio capture is not implemented"_s });
#endif
}

void SpeechRecognitionServer::stop(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    MESSAGE_CHECK(clientIdentifier);

    if (m_requests.remove(clientIdentifier)) {
        sendUpdate(clientIdentifier, WebCore::SpeechRecognitionUpdateType::End);
        return;
    }

    if (m_recognizer && m_recognizer->clientIdentifier() == clientIdentifier)
        m_recognizer->stop();
}

void SpeechRecognitionServer::abort(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    MESSAGE_CHECK(clientIdentifier);
    if (m_requests.remove(clientIdentifier)) {
        sendUpdate(clientIdentifier, WebCore::SpeechRecognitionUpdateType::End);
        return;
    }

    if (m_recognizer && m_recognizer->clientIdentifier() == clientIdentifier)
        m_recognizer->abort();
}

void SpeechRecognitionServer::invalidate(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    MESSAGE_CHECK(clientIdentifier);
    if (m_recognizer && m_recognizer->clientIdentifier() == clientIdentifier)
        m_recognizer->abort();
}

void SpeechRecognitionServer::sendUpdate(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier, WebCore::SpeechRecognitionUpdateType type, std::optional<WebCore::SpeechRecognitionError> error, std::optional<Vector<WebCore::SpeechRecognitionResultData>> result)
{
    auto update = WebCore::SpeechRecognitionUpdate::create(clientIdentifier, type);
    if (type == WebCore::SpeechRecognitionUpdateType::Error)
        update = WebCore::SpeechRecognitionUpdate::createError(clientIdentifier, *error);
    if (type == WebCore::SpeechRecognitionUpdateType::Result)
        update = WebCore::SpeechRecognitionUpdate::createResult(clientIdentifier, *result);
    sendUpdate(update);
}

void SpeechRecognitionServer::sendUpdate(const WebCore::SpeechRecognitionUpdate& update)
{
    send(Messages::WebSpeechRecognitionConnection::DidReceiveUpdate(update));
}

IPC::Connection* SpeechRecognitionServer::messageSenderConnection() const
{
    return m_connection.ptr();
}

uint64_t SpeechRecognitionServer::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
}

void SpeechRecognitionServer::mute()
{
    if (!m_recognizer)
        return;
    
    if (auto* source = m_recognizer->source())
        source->mute();
}

} // namespace WebKit

#undef MESSAGE_CHECK
