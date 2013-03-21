/*
 * Copyright (C) 2013 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
OBJC_CLASS WebSpeechSynthesisWrapper;
#endif

namespace WebCore {

enum SpeechBoundary {
    SpeechWordBoundary,
    SpeechSentenceBoundary
};

class PlatformSpeechSynthesisUtterance;

class PlatformSpeechSynthesizerClient {
public:
    virtual void didStartSpeaking(const PlatformSpeechSynthesisUtterance*) = 0;
    virtual void didFinishSpeaking(const PlatformSpeechSynthesisUtterance*) = 0;
    virtual void didPauseSpeaking(const PlatformSpeechSynthesisUtterance*) = 0;
    virtual void didResumeSpeaking(const PlatformSpeechSynthesisUtterance*) = 0;
    virtual void speakingErrorOccurred(const PlatformSpeechSynthesisUtterance*) = 0;
    virtual void boundaryEventOccurred(const PlatformSpeechSynthesisUtterance*, SpeechBoundary, unsigned charIndex) = 0;
    virtual void voicesDidChange() = 0;
protected:
    virtual ~PlatformSpeechSynthesizerClient() { }
};
    
class PlatformSpeechSynthesizer {
public:
    static PassOwnPtr<PlatformSpeechSynthesizer> create(PlatformSpeechSynthesizerClient*);

    virtual ~PlatformSpeechSynthesizer() { }
    
    const Vector<RefPtr<PlatformSpeechSynthesisVoice> >& voiceList() const { return m_voiceList; }
    virtual void speak(const PlatformSpeechSynthesisUtterance&);
    virtual void pause();
    virtual void resume();
    virtual void cancel();
    
    PlatformSpeechSynthesizerClient* client() const { return m_speechSynthesizerClient; }
    
protected:
    explicit PlatformSpeechSynthesizer(PlatformSpeechSynthesizerClient*);
    Vector<RefPtr<PlatformSpeechSynthesisVoice> > m_voiceList;
    
private:
    PlatformSpeechSynthesizerClient* m_speechSynthesizerClient;
    virtual void initializeVoiceList();
    
#if PLATFORM(MAC)
    RetainPtr<WebSpeechSynthesisWrapper> m_platformSpeechWrapper;
#endif
};
    
} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)

#endif // PlatformSpeechSynthesizer_h
