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

#import "config.h"
#import "SpeechRecognizer.h"

#if HAVE(SPEECHRECOGNIZER)

#import "MediaUtilities.h"
#import "SpeechRecognitionUpdate.h"
#import "WebSpeechRecognizerTaskMock.h"
#import <Speech/Speech.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>

namespace WebCore {

void SpeechRecognizer::dataCaptured(const MediaTime& time, const PlatformAudioData& data, const AudioStreamDescription& description, size_t sampleCount)
{
    auto buffer = createAudioSampleBuffer(data, description, PAL::toCMTime(time), sampleCount);
    [m_task audioSamplesAvailable:buffer.get()];
}

bool SpeechRecognizer::startRecognition(bool mockSpeechRecognitionEnabled, SpeechRecognitionConnectionClientIdentifier identifier, const String& localeIdentifier, bool continuous, bool interimResults, uint64_t alternatives)
{
    auto taskClass = mockSpeechRecognitionEnabled ? [WebSpeechRecognizerTaskMock class] : [WebSpeechRecognizerTask class];
    m_task = adoptNS([[taskClass alloc] initWithIdentifier:identifier locale:localeIdentifier doMultipleRecognitions:continuous reportInterimResults:interimResults maxAlternatives:alternatives delegateCallback:[this, weakThis = makeWeakPtr(this)](const WebCore::SpeechRecognitionUpdate& update) {
        if (!weakThis)
            return;

        m_delegateCallback(update);
    }]);

    return !!m_task;
}

void SpeechRecognizer::stopRecognition()
{
    ASSERT(m_task);
    [m_task stop];
}

void SpeechRecognizer::abortRecognition()
{
    ASSERT(m_task);
    [m_task abort];
}

void SpeechRecognizer::resetRecognition()
{
    if (!m_task)
        return;

    auto task = std::exchange(m_task, nullptr);
    [task abort];
}

} // namespace WebCore

#endif // HAVE(SPEECHRECOGNIZER)
