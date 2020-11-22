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

#if PLATFORM(COCOA)
#include "MediaUtilities.h"
#include <pal/avfoundation/MediaTimeAVFoundation.h>
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

    if (m_source)
        m_source = nullptr;

    auto error = SpeechRecognitionError { SpeechRecognitionErrorType::Aborted, "Another request is started" };
    m_delegateCallback(SpeechRecognitionUpdate::createError(*m_clientIdentifier, error));
}

void SpeechRecognizer::start(SpeechRecognitionConnectionClientIdentifier identifier)
{
    reset();

    m_clientIdentifier = identifier;
    m_delegateCallback(SpeechRecognitionUpdate::create(*m_clientIdentifier, SpeechRecognitionUpdateType::Start));

    startInternal();
}

void SpeechRecognizer::startInternal()
{
    auto dataCallback = [weakThis = makeWeakPtr(this)](const auto& time, const auto& data, const auto& description, auto sampleCount) {
        if (!weakThis)
            return;

#if PLATFORM(COCOA)
        auto buffer = createAudioSampleBuffer(data, description, PAL::toCMTime(time), sampleCount);
        UNUSED_PARAM(buffer);
#else
        UNUSED_PARAM(time);
        UNUSED_PARAM(data);
        UNUSED_PARAM(description);
        UNUSED_PARAM(sampleCount);
#endif
    };

    auto stateUpdateCallback = [this, weakThis = makeWeakPtr(this)](const auto& update) {
        if (!weakThis)
            return;

        ASSERT(m_clientIdentifier && m_clientIdentifier.value() == update.clientIdentifier());
        m_delegateCallback(update);

        if (update.type() == SpeechRecognitionUpdateType::Error)
            m_source = nullptr;
    };

    m_source = makeUnique<SpeechRecognitionCaptureSource>(*m_clientIdentifier, WTFMove(dataCallback), WTFMove(stateUpdateCallback));
}

void SpeechRecognizer::stop(ShouldGenerateFinalResult shouldGenerateFinalResult)
{
    if (!m_clientIdentifier)
        return;

    stopInternal();

    if (shouldGenerateFinalResult == ShouldGenerateFinalResult::Yes) {
        // TODO: generate real result when speech recognition backend is implemented.
        Vector<SpeechRecognitionResultData> resultDatas;
        m_delegateCallback(SpeechRecognitionUpdate::createResult(*m_clientIdentifier, resultDatas));
    }

    m_delegateCallback(SpeechRecognitionUpdate::create(*m_clientIdentifier, SpeechRecognitionUpdateType::End));
    m_clientIdentifier = WTF::nullopt;
}

void SpeechRecognizer::stopInternal()
{
    if (!m_source)
        return;

    m_source = nullptr;
    m_delegateCallback(SpeechRecognitionUpdate::create(*m_clientIdentifier, SpeechRecognitionUpdateType::AudioEnd));
}

} // namespace WebCore
