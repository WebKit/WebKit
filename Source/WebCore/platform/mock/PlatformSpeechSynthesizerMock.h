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

#ifndef PlatformSpeechSynthesizerMock_h
#define PlatformSpeechSynthesizerMock_h

#if ENABLE(SPEECH_SYNTHESIS)

#include "PlatformSpeechSynthesizer.h"
#include "Timer.h"

namespace WebCore {

class PlatformSpeechSynthesizerMock : public PlatformSpeechSynthesizer {
public:
    WEBCORE_EXPORT explicit PlatformSpeechSynthesizerMock(PlatformSpeechSynthesizerClient*);

    virtual ~PlatformSpeechSynthesizerMock();
    virtual void speak(RefPtr<PlatformSpeechSynthesisUtterance>&&);
    virtual void pause();
    virtual void resume();
    virtual void cancel();

private:
    virtual void initializeVoiceList();
    void speakingFinished();

    Timer m_speakingFinishedTimer;
    RefPtr<PlatformSpeechSynthesisUtterance> m_utterance;
};

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)

#endif // PlatformSpeechSynthesizer_h
