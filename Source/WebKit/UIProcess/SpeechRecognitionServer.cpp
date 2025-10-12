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

#include "MessageSenderInlines.h"
#include "UserMediaProcessManager.h"
#include "WebProcessProxy.h"
#include "WebSpeechRecognitionConnectionMessages.h"
#include <WebCore/SpeechRecognitionRequestInfo.h>
#include <WebCore/SpeechRecognitionUpdate.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, *messageSenderConnection())

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeechRecognitionServer);

Ref<SpeechRecognitionServer> SpeechRecognitionServer::create(WebProcessProxy& process, SpeechRecognitionServerIdentifier identifier, SpeechRecognitionPermissionChecker&& permissionChecker, SpeechRecognitionCheckIfMockSpeechRecognitionEnabled&& checkIfEnabled
#if ENABLE(MEDIA_STREAM)
    , RealtimeMediaSourceCreateFunction&& function
#endif
    )
{
    return adoptRef(*new SpeechRecognitionServer(process, identifier, WTFMove(permissionChecker), WTFMove(checkIfEnabled)
#if ENABLE(MEDIA_STREAM)
        , WTFMove(function)
#endif
    ));
}

SpeechRecognitionServer::SpeechRecognitionServer(WebProcessProxy& process, SpeechRecognitionServerIdentifier identifier, SpeechRecognitionPermissionChecker&& permissionChecker, SpeechRecognitionCheckIfMockSpeechRecognitionEnabled&& checkIfEnabled
#if ENABLE(MEDIA_STREAM)
    , RealtimeMediaSourceCreateFunction&& function
#endif
    )
    : m_process(process)
    , m_identifier(identifier)
    , m_permissionChecker(WTFMove(permissionChecker))
    , m_checkIfMockSpeechRecognitionEnabled(WTFMove(checkIfEnabled))
#if ENABLE(MEDIA_STREAM)
    , m_realtimeMediaSourceCreateFunction(WTFMove(function))
#endif
{
}

std::optional<SharedPreferencesForWebProcess> SpeechRecognitionServer::sharedPreferencesForWebProcess() const
{
    return m_process ? m_process->sharedPreferencesForWebProcess() : std::nullopt;
}

void SpeechRecognitionServer::start(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier, String&& lang, bool continuous, bool interimResults, uint64_t maxAlternatives, WebCore::ClientOrigin&& origin, WebCore::FrameIdentifier mainFrameIdentifier, FrameInfoData&& frameInfo)
{
    ASSERT(!m_requests.contains(clientIdentifier));
    auto requestInfo = WebCore::SpeechRecognitionRequestInfo { clientIdentifier, WTFMove(lang), continuous, interimResults, maxAlternatives, WTFMove(origin), mainFrameIdentifier };
    auto& newRequest = m_requests.add(clientIdentifier, makeUnique<WebCore::SpeechRecognitionRequest>(WTFMove(requestInfo))).iterator->value;

    requestPermissionForRequest(*newRequest, WTFMove(frameInfo));
}

void SpeechRecognitionServer::requestPermissionForRequest(WebCore::SpeechRecognitionRequest& request, FrameInfoData&& frameInfo)
{
    m_permissionChecker(request, WTFMove(frameInfo), [weakThis = WeakPtr { *this }, weakRequest = WeakPtr { request }](auto error) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !weakRequest)
            return;

        auto identifier = weakRequest->clientIdentifier();
        auto request = protectedThis->m_requests.take(identifier);
        if (error) {
            protectedThis->sendUpdate(identifier, WebCore::SpeechRecognitionUpdateType::Error, WTFMove(error));
            return;
        }

        ASSERT(request);
        protectedThis->handleRequest(makeUniqueRefFromNonNullUniquePtr(WTFMove(request)));
    });
}

CheckedPtr<WebCore::SpeechRecognizer> SpeechRecognitionServer::checkedRecognizer() const
{
    return m_recognizer.get();
}

void SpeechRecognitionServer::handleRequest(UniqueRef<WebCore::SpeechRecognitionRequest>&& request)
{
    if (CheckedPtr recognizer = m_recognizer.get()) {
        recognizer->abort(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::Aborted, "Another request is started"_s });
        recognizer->prepareForDestruction();
    }

    auto clientIdentifier = request->clientIdentifier();
    m_recognizer = makeUnique<WebCore::SpeechRecognizer>([weakThis = WeakPtr { *this }](auto& update) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->sendUpdate(update);

        if (update.type() == WebCore::SpeechRecognitionUpdateType::Error)
            protectedThis->checkedRecognizer()->abort();
        else if (update.type() == WebCore::SpeechRecognitionUpdateType::End)
            protectedThis->checkedRecognizer()->setInactive();
    }, WTFMove(request));

#if ENABLE(MEDIA_STREAM)
    auto sourceOrError = m_realtimeMediaSourceCreateFunction();
    if (!sourceOrError) {
        sendUpdate(WebCore::SpeechRecognitionUpdate::createError(clientIdentifier, WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::AudioCapture, sourceOrError.error.errorMessage }));
        return;
    }

    WebProcessProxy::muteCaptureInPagesExcept(m_identifier);
    bool mockDeviceCapturesEnabled = m_checkIfMockSpeechRecognitionEnabled();
    checkedRecognizer()->start(sourceOrError.source(), mockDeviceCapturesEnabled);
#else
    sendUpdate(clientIdentifier, WebCore::SpeechRecognitionUpdateType::Error, WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::AudioCapture, "Audio capture is not implemented"_s });
#endif
}

void SpeechRecognitionServer::stop(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    if (m_requests.remove(clientIdentifier)) {
        sendUpdate(clientIdentifier, WebCore::SpeechRecognitionUpdateType::End);
        return;
    }

    if (CheckedPtr recognizer = m_recognizer.get()) {
        if (recognizer->clientIdentifier() == clientIdentifier)
            recognizer->stop();
    }
}

void SpeechRecognitionServer::abort(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    if (m_requests.remove(clientIdentifier)) {
        sendUpdate(clientIdentifier, WebCore::SpeechRecognitionUpdateType::End);
        return;
    }

    if (CheckedPtr recognizer = m_recognizer.get()) {
        if (recognizer->clientIdentifier() == clientIdentifier)
            recognizer->abort();
    }
}

void SpeechRecognitionServer::invalidate(WebCore::SpeechRecognitionConnectionClientIdentifier clientIdentifier)
{
    if (CheckedPtr recognizer = m_recognizer.get()) {
        if (recognizer->clientIdentifier() == clientIdentifier)
            recognizer->abort();
    }
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
    return m_process ? &m_process->connection() : nullptr;
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
