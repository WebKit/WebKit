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

#ifndef WebSpeechRecognizer_h
#define WebSpeechRecognizer_h

#include "WebSpeechRecognitionHandle.h"
#include "platform/WebCommon.h"

namespace WebKit {

class WebSpeechGrammar;
class WebSpeechRecognitionParams;
class WebSpeechRecognizerClient;

// Interface for speech recognition, to be implemented by the embedder.
class WebSpeechRecognizer {
public:
    // Start speech recognition for the specified handle using the specified parameters. Notifications on progress, results, and errors will be sent via the client.
    virtual void start(const WebSpeechRecognitionHandle&, const WebSpeechRecognitionParams&, WebSpeechRecognizerClient*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Stop speech recognition for the specified handle, returning any results for the audio recorded so far. Notifications and errors are sent via the client.
    virtual void stop(const WebSpeechRecognitionHandle&, WebSpeechRecognizerClient*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Abort speech recognition for the specified handle, discarding any recorded audio. Notifications and errors are sent via the client.
    virtual void abort(const WebSpeechRecognitionHandle&, WebSpeechRecognizerClient*) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    virtual ~WebSpeechRecognizer() { }
};

} // namespace WebKit

#endif // WebSpeechRecognizer_h
