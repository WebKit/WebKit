/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformSpeechSynthesizerMock.h"
#include "PlatformSpeechSynthesisUtterance.h"

#if ENABLE(SPEECH_SYNTHESIS)

namespace WebCore {

PlatformSpeechSynthesizerMock::PlatformSpeechSynthesizerMock(PlatformSpeechSynthesizerClient* client)
    : PlatformSpeechSynthesizer(client)
    , m_speakingFinishedTimer(*this, &PlatformSpeechSynthesizerMock::speakingFinished)
{
}

PlatformSpeechSynthesizerMock::~PlatformSpeechSynthesizerMock() = default;

void PlatformSpeechSynthesizerMock::speakingFinished()
{
    ASSERT(m_utterance.get());
    RefPtr<PlatformSpeechSynthesisUtterance> protect(m_utterance);
    m_utterance = nullptr;

    client()->didFinishSpeaking(*protect);
}

void PlatformSpeechSynthesizerMock::initializeVoiceList()
{
    m_voiceList.append(PlatformSpeechSynthesisVoice::create(String("mock.voice.bruce"), String("bruce"), String("en-US"), true, true));
    m_voiceList.append(PlatformSpeechSynthesisVoice::create(String("mock.voice.clark"), String("clark"), String("en-US"), true, false));
    m_voiceList.append(PlatformSpeechSynthesisVoice::create(String("mock.voice.logan"), String("logan"), String("fr-CA"), true, true));
}

void PlatformSpeechSynthesizerMock::speak(RefPtr<PlatformSpeechSynthesisUtterance>&& utterance)
{
    ASSERT(!m_utterance);
    m_utterance = WTFMove(utterance);
    client()->didStartSpeaking(*m_utterance);

    // Fire a fake word and then sentence boundary event.
    client()->boundaryEventOccurred(*m_utterance, SpeechBoundary::SpeechWordBoundary, 0);
    client()->boundaryEventOccurred(*m_utterance, SpeechBoundary::SpeechSentenceBoundary, m_utterance->text().length());

    // Give the fake speech job some time so that pause and other functions have time to be called.
    m_speakingFinishedTimer.startOneShot(100_ms);
}

void PlatformSpeechSynthesizerMock::cancel()
{
    if (!m_utterance)
        return;

    m_speakingFinishedTimer.stop();
    client()->speakingErrorOccurred(*m_utterance);
    m_utterance = nullptr;
}

void PlatformSpeechSynthesizerMock::pause()
{
    client()->didPauseSpeaking(*m_utterance);
}

void PlatformSpeechSynthesizerMock::resume()
{
    client()->didResumeSpeaking(*m_utterance);
}

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)
