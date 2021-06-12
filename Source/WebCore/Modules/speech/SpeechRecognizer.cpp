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
#include "SpeechRecognizer.h"

#include "SpeechRecognitionRequest.h"
#include "SpeechRecognitionUpdate.h"
#include <wtf/MediaTime.h>

#if PLATFORM(COCOA)
#include "MediaUtilities.h"
#endif

namespace WebCore {

SpeechRecognizer::SpeechRecognizer(DelegateCallback&& delegateCallback, UniqueRef<SpeechRecognitionRequest>&& request)
    : m_delegateCallback(WTFMove(delegateCallback))
    , m_request(WTFMove(request))
{
}

void SpeechRecognizer::abort(std::optional<SpeechRecognitionError>&& error)
{
    if (m_state == State::Aborting || m_state == State::Inactive)
        return;
    m_state = State::Aborting;

    if (error)
        m_delegateCallback(SpeechRecognitionUpdate::createError(clientIdentifier(), *error));

    stopCapture();
    abortRecognition();
}

void SpeechRecognizer::stop()
{
    if (m_state == State::Aborting || m_state == State::Inactive)
        return;
    m_state = State::Stopping;

    stopCapture();
    stopRecognition();
}

SpeechRecognitionConnectionClientIdentifier SpeechRecognizer::clientIdentifier() const
{
    return m_request->clientIdentifier();
}

void SpeechRecognizer::prepareForDestruction()
{
    if (m_state == State::Inactive)
        return;

    auto delegateCallback = std::exchange(m_delegateCallback, [](const SpeechRecognitionUpdate&) { });
    delegateCallback(SpeechRecognitionUpdate::create(clientIdentifier(), SpeechRecognitionUpdateType::End));
    m_state = State::Inactive;
}

#if ENABLE(MEDIA_STREAM)

void SpeechRecognizer::start(Ref<RealtimeMediaSource>&& source, bool mockSpeechRecognitionEnabled)
{
    if (!startRecognition(mockSpeechRecognitionEnabled, clientIdentifier(), m_request->lang(), m_request->continuous(), m_request->interimResults(), m_request->maxAlternatives())) {
        auto error = SpeechRecognitionError { SpeechRecognitionErrorType::ServiceNotAllowed, "Failed to start recognition"_s };
        m_delegateCallback(SpeechRecognitionUpdate::createError(clientIdentifier(), WTFMove(error)));
        return;
    }

    m_state = State::Running;
    m_delegateCallback(SpeechRecognitionUpdate::create(clientIdentifier(), SpeechRecognitionUpdateType::Start));
    startCapture(WTFMove(source));
}

void SpeechRecognizer::startCapture(Ref<RealtimeMediaSource>&& source)
{
    auto dataCallback = [weakThis = makeWeakPtr(this)](const auto& time, const auto& data, const auto& description, auto sampleCount) {
        if (weakThis)
            weakThis->dataCaptured(time, data, description, sampleCount);
    };

    auto stateUpdateCallback = [weakThis = makeWeakPtr(this)](const auto& update) {
        if (weakThis)
            weakThis->m_delegateCallback(update);
    };

    m_source = makeUnique<SpeechRecognitionCaptureSource>(clientIdentifier(), WTFMove(dataCallback), WTFMove(stateUpdateCallback), WTFMove(source));
}

#endif

void SpeechRecognizer::stopCapture()
{
    if (!m_source)
        return;

    m_source = nullptr;
    m_delegateCallback(SpeechRecognitionUpdate::create(clientIdentifier(), SpeechRecognitionUpdateType::AudioEnd));
}

#if !HAVE(SPEECHRECOGNIZER)

void SpeechRecognizer::dataCaptured(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t)
{
}

bool SpeechRecognizer::startRecognition(bool, SpeechRecognitionConnectionClientIdentifier, const String&, bool, bool, uint64_t)
{
    return true;
}

void SpeechRecognizer::abortRecognition()
{
    m_delegateCallback(SpeechRecognitionUpdate::create(clientIdentifier(), SpeechRecognitionUpdateType::End));
}

void SpeechRecognizer::stopRecognition()
{
    m_delegateCallback(SpeechRecognitionUpdate::create(clientIdentifier(), SpeechRecognitionUpdateType::End));
}

#endif

} // namespace WebCore
