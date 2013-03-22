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

#ifndef WebSpeechRecognizerClient_h
#define WebSpeechRecognizerClient_h

#include "../../../Platform/chromium/public/WebVector.h"

namespace WebKit {

class WebSpeechRecognitionResult;
class WebSpeechRecognitionHandle;
class WebString;

// A client for reporting progress on speech recognition for a specific handle.
class WebSpeechRecognizerClient {
public:
    enum ErrorCode {
        OtherError = 0,
        NoSpeechError = 1,
        AbortedError = 2,
        AudioCaptureError = 3,
        NetworkError = 4,
        NotAllowedError = 5,
        ServiceNotAllowedError = 6,
        BadGrammarError = 7,
        LanguageNotSupportedError = 8
    };

    // These methods correspond to the events described in the spec:
    // http://speech-javascript-api-spec.googlecode.com/git/speechapi.html#speechreco-events

    // To be called when the embedder has started to capture audio.
    virtual void didStartAudio(const WebSpeechRecognitionHandle&) = 0;

    // To be called when some sound, possibly speech, has been detected.
    // This is expected to be called after didStartAudio.
    virtual void didStartSound(const WebSpeechRecognitionHandle&) = 0;

    // To be called when sound is no longer detected.
    // This is expected to be called after didEndSpeech.
    virtual void didEndSound(const WebSpeechRecognitionHandle&) = 0;

    // To be called when audio capture has stopped.
    // This is expected to be called after didEndSound.
    virtual void didEndAudio(const WebSpeechRecognitionHandle&) = 0;

    // To be called when the speech recognizer provides new results.
    // - newFinalResults contains zero or more final results that are new since
    // the last time the function was called.
    // - currentInterimResults contains zero or more inteirm results that
    // replace the interim results that were reported the last time this
    // function was called.
    virtual void didReceiveResults(const WebSpeechRecognitionHandle&, const WebVector<WebSpeechRecognitionResult>& newFinalResults, const WebVector<WebSpeechRecognitionResult>& currentInterimResults) = 0;

    // To be called when the speech recognizer returns a final result with no
    // recognizion hypothesis.
    virtual void didReceiveNoMatch(const WebSpeechRecognitionHandle&, const WebSpeechRecognitionResult&) = 0;

    // To be called when a speech recognition error occurs.
    virtual void didReceiveError(const WebSpeechRecognitionHandle&, const WebString& message, ErrorCode) = 0;

    // To be called when the recognizer has begun to listen to the audio with
    // the intention of recognizing.
    virtual void didStart(const WebSpeechRecognitionHandle&) = 0;

    // To be called when the recognition session has ended. This must always be
    // called, no matter the reason for the end.
    virtual void didEnd(const WebSpeechRecognitionHandle&) = 0;

protected:
    virtual ~WebSpeechRecognizerClient() {}
};

} // namespace WebKit

#endif // WebSpeechRecognizerClient_h
