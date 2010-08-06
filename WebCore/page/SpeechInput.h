/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SpeechInput_h
#define SpeechInput_h

#if ENABLE(INPUT_SPEECH)

#include "SpeechInputListener.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class SpeechInputClient;
class SpeechInputListener;
class String;

// This class connects the input elements requiring speech input with the platform specific
// speech recognition engine. It provides methods for the input elements to activate speech
// recognition and methods for the speech recognition engine to return back the results.
class SpeechInput : public Noncopyable, public SpeechInputListener {
public:
    SpeechInput(SpeechInputClient*);
    virtual ~SpeechInput() { }

    // Methods invoked by the input elements.
    bool startRecognition(SpeechInputListener* listener);
    void stopRecording();

    // SpeechInputListener methods.
    virtual void didCompleteRecording();
    virtual void didCompleteRecognition();
    virtual void setRecognitionResult(const String&);

private:
    SpeechInputClient* m_client;
    SpeechInputListener* m_listener;
};

} // namespace WebCore

#endif // ENABLE(INPUT_SPEECH)

#endif // SpeechInput_h
