/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebSpeechSynthesizerClientImpl_h
#define WebSpeechSynthesizerClientImpl_h

#include "PlatformSpeechSynthesizer.h"
#include <public/WebSpeechSynthesisUtterance.h>
#include <public/WebSpeechSynthesisVoice.h>
#include <public/WebSpeechSynthesizerClient.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class PlatformSpeechSynthesizer;
class PlatformSpeechSynthesizerClient;

class WebSpeechSynthesizerClientImpl : public WebKit::WebSpeechSynthesizerClient {
public:
    explicit WebSpeechSynthesizerClientImpl(PlatformSpeechSynthesizer*, PlatformSpeechSynthesizerClient*);
    virtual ~WebSpeechSynthesizerClientImpl();

    virtual void setVoiceList(const WebKit::WebVector<WebKit::WebSpeechSynthesisVoice>& voices);
    virtual void didStartSpeaking(const WebKit::WebSpeechSynthesisUtterance&);
    virtual void didFinishSpeaking(const WebKit::WebSpeechSynthesisUtterance&);
    virtual void didPauseSpeaking(const WebKit::WebSpeechSynthesisUtterance&);
    virtual void didResumeSpeaking(const WebKit::WebSpeechSynthesisUtterance&);
    virtual void speakingErrorOccurred(const WebKit::WebSpeechSynthesisUtterance&);
    virtual void wordBoundaryEventOccurred(const WebKit::WebSpeechSynthesisUtterance&, unsigned charIndex);
    virtual void sentenceBoundaryEventOccurred(const WebKit::WebSpeechSynthesisUtterance&, unsigned charIndex);

private:
#if ENABLE(SPEECH_SYNTHESIS)
    PlatformSpeechSynthesizer* m_synthesizer;
    PlatformSpeechSynthesizerClient* m_client;
#endif
};

} // namespace WebCore

#endif // WebSpeechSynthesizerClientImpl_h
