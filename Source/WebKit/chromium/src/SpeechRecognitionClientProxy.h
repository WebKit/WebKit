/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef SpeechRecognitionClientProxy_h
#define SpeechRecognitionClientProxy_h

#include "PlatformString.h"
#include "SpeechRecognitionClient.h"
#include "WebSpeechRecognizerClient.h"
#include <wtf/Compiler.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class WebSpeechRecognizer;
class WebString;

class SpeechRecognitionClientProxy : public WebCore::SpeechRecognitionClient, public WebSpeechRecognizerClient {
public:
    ~SpeechRecognitionClientProxy();

    // Constructing a proxy object with a 0 WebSpeechRecognizer is safe in
    // itself, but attempting to call start/stop/abort on it will crash.
    static PassOwnPtr<SpeechRecognitionClientProxy> create(WebSpeechRecognizer*);

    // WebCore::SpeechRecognitionClient:
    virtual void start(WebCore::SpeechRecognition*, const WebCore::SpeechGrammarList*, const String& lang, bool continuous) OVERRIDE;
    virtual void stop(WebCore::SpeechRecognition*) OVERRIDE;
    virtual void abort(WebCore::SpeechRecognition*) OVERRIDE;

    // WebSpeechRecognizerClient:
    virtual void didStartAudio(const WebSpeechRecognitionHandle&) OVERRIDE;
    virtual void didStartSound(const WebSpeechRecognitionHandle&) OVERRIDE;
    virtual void didStartSpeech(const WebSpeechRecognitionHandle&) OVERRIDE;
    virtual void didEndSpeech(const WebSpeechRecognitionHandle&) OVERRIDE;
    virtual void didEndSound(const WebSpeechRecognitionHandle&) OVERRIDE;
    virtual void didEndAudio(const WebSpeechRecognitionHandle&) OVERRIDE;
    virtual void didReceiveResult(const WebSpeechRecognitionHandle&, const WebSpeechRecognitionResult&, unsigned long resultIndex, const WebVector<WebSpeechRecognitionResult>& resultHistory) OVERRIDE;
    virtual void didReceiveNoMatch(const WebSpeechRecognitionHandle&, const WebSpeechRecognitionResult&) OVERRIDE;
    virtual void didDeleteResult(const WebSpeechRecognitionHandle&, unsigned resultIndex, const WebVector<WebSpeechRecognitionResult>& resultHistory) OVERRIDE;
    virtual void didReceiveError(const WebSpeechRecognitionHandle&, const WebString& message, WebSpeechRecognizerClient::ErrorCode) OVERRIDE;
    virtual void didStart(const WebSpeechRecognitionHandle&) OVERRIDE;
    virtual void didEnd(const WebSpeechRecognitionHandle&) OVERRIDE;

private:
    SpeechRecognitionClientProxy(WebSpeechRecognizer*);

    WebSpeechRecognizer* m_recognizer;
};

}; // namespace WebKit

#endif // SpeechRecognitionClientProxy_h
