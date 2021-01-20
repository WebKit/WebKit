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

#include "SpeechRecognitionUpdate.h"
#include <wtf/MediaTime.h>

#if PLATFORM(COCOA)
#include "MediaUtilities.h"
#endif

namespace WebCore {

SpeechRecognizer::SpeechRecognizer(DelegateCallback&& callback)
    : m_delegateCallback(WTFMove(callback))
{
}

void SpeechRecognizer::reset()
{
    if (!m_clientIdentifier)
        return;

    stopCapture();
    resetRecognition();
    m_clientIdentifier = WTF::nullopt;
}

void SpeechRecognizer::abort()
{
    ASSERT(m_clientIdentifier);
    stopCapture();
    abortRecognition();
}

void SpeechRecognizer::stop()
{
    ASSERT(m_clientIdentifier);
    stopCapture();
    stopRecognition();
}

#if ENABLE(MEDIA_STREAM)

void SpeechRecognizer::start(SpeechRecognitionConnectionClientIdentifier clientIdentifier, Ref<RealtimeMediaSource>&& source, bool mockSpeechRecognitionEnabled, const String& localeIdentifier, bool continuous, bool interimResults, uint64_t maxAlternatives)
{
    ASSERT(!m_clientIdentifier);
    m_clientIdentifier = clientIdentifier;
    m_delegateCallback(SpeechRecognitionUpdate::create(*m_clientIdentifier, SpeechRecognitionUpdateType::Start));

    if (!startRecognition(mockSpeechRecognitionEnabled, clientIdentifier, localeIdentifier, continuous, interimResults, maxAlternatives)) {
        auto error = WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::ServiceNotAllowed, "Failed to start recognition"_s };
        m_delegateCallback(WebCore::SpeechRecognitionUpdate::createError(clientIdentifier, WTFMove(error)));
        return;
    }

    startCapture(WTFMove(source));
}

void SpeechRecognizer::startCapture(Ref<RealtimeMediaSource>&& source)
{
    auto dataCallback = [weakThis = makeWeakPtr(this)](const auto& time, const auto& data, const auto& description, auto sampleCount) {
        if (weakThis)
            weakThis->dataCaptured(time, data, description, sampleCount);
    };

    auto stateUpdateCallback = [this, weakThis = makeWeakPtr(this)](const auto& update) {
        if (!weakThis)
            return;

        ASSERT(m_clientIdentifier && m_clientIdentifier.value() == update.clientIdentifier());
        m_delegateCallback(update);
    };

    m_source = makeUnique<SpeechRecognitionCaptureSource>(*m_clientIdentifier, WTFMove(dataCallback), WTFMove(stateUpdateCallback), WTFMove(source));
}

#endif

void SpeechRecognizer::stopCapture()
{
    if (!m_source)
        return;

    m_source = nullptr;
    m_delegateCallback(SpeechRecognitionUpdate::create(*m_clientIdentifier, SpeechRecognitionUpdateType::AudioEnd));
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
    m_delegateCallback(SpeechRecognitionUpdate::create(*m_clientIdentifier, SpeechRecognitionUpdateType::End));
}

void SpeechRecognizer::stopRecognition()
{
    m_delegateCallback(SpeechRecognitionUpdate::create(*m_clientIdentifier, SpeechRecognitionUpdateType::End));
}

void SpeechRecognizer::resetRecognition()
{
    abortRecognition();
}

#endif

} // namespace WebCore
