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

#ifndef PlatformSpeechSynthesizer_h
#define PlatformSpeechSynthesizer_h

#if ENABLE(SPEECH_SYNTHESIS)

#include "PlatformSpeechSynthesisVoice.h"
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS WebSpeechSynthesisWrapper;
#endif

namespace WebCore {

enum class SpeechBoundary : uint8_t {
    SpeechWordBoundary,
    SpeechSentenceBoundary
};

class PlatformSpeechSynthesisUtterance;

class PlatformSpeechSynthesizerClient {
public:
    virtual void didStartSpeaking(PlatformSpeechSynthesisUtterance&) = 0;
    virtual void didFinishSpeaking(PlatformSpeechSynthesisUtterance&) = 0;
    virtual void didPauseSpeaking(PlatformSpeechSynthesisUtterance&) = 0;
    virtual void didResumeSpeaking(PlatformSpeechSynthesisUtterance&) = 0;
    virtual void speakingErrorOccurred(PlatformSpeechSynthesisUtterance&) = 0;
    virtual void boundaryEventOccurred(PlatformSpeechSynthesisUtterance&, SpeechBoundary, unsigned charIndex) = 0;
    virtual void voicesDidChange() = 0;
protected:
    virtual ~PlatformSpeechSynthesizerClient() = default;
};

class WEBCORE_EXPORT PlatformSpeechSynthesizer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT explicit PlatformSpeechSynthesizer(PlatformSpeechSynthesizerClient*);

    // FIXME: We have multiple virtual functions just so we can support a mock for testing.
    // Seems wasteful. Would be nice to find a better way.
    WEBCORE_EXPORT virtual ~PlatformSpeechSynthesizer();

    const Vector<RefPtr<PlatformSpeechSynthesisVoice>>& voiceList() const;
    virtual void speak(RefPtr<PlatformSpeechSynthesisUtterance>&&);
    virtual void pause();
    virtual void resume();
    virtual void cancel();
    virtual void resetState();

    PlatformSpeechSynthesizerClient* client() const { return m_speechSynthesizerClient; }

protected:
    Vector<RefPtr<PlatformSpeechSynthesisVoice>> m_voiceList;

private:
    virtual void initializeVoiceList();

    bool m_voiceListIsInitialized { false };
    PlatformSpeechSynthesizerClient* m_speechSynthesizerClient;

#if PLATFORM(COCOA)
    RetainPtr<WebSpeechSynthesisWrapper> m_platformSpeechWrapper;
#endif
};

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)

#endif // PlatformSpeechSynthesizer_h
