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

#ifndef WebSpeechInputController_h
#define WebSpeechInputController_h

#include "WebCommon.h"

namespace WebKit {

class WebString;
struct WebRect;
class WebString;

// Provides an embedder API called by WebKit.
class WebSpeechInputController {
public:
    // Starts speech recognition. Speech will get recorded until the endpointer detects silence,
    // runs to the limit or stopRecording is called. Progress indications and the recognized
    // text are returned via the listener interface.
    virtual bool startRecognition(int requestId, const WebRect& elementRect, const WebString& language, const WebString& grammar)
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return false;
    }

    // Cancels an ongoing recognition and discards any audio recorded so far. No partial
    // recognition results are returned to the listener.
    virtual void cancelRecognition(int) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Stops audio recording and performs recognition with the audio recorded until now
    // (does not discard audio). This is an optional call and is typically invoked if the user
    // wants to stop recording audio as soon as they finished speaking. Otherwise, the speech
    // recording 'endpointer' should detect silence in the input and stop recording automatically.
    // Call startRecognition() to record audio and recognize speech again.
    virtual void stopRecording(int) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    virtual ~WebSpeechInputController() { }
};

} // namespace WebKit

#endif // WebSpeechInputController_h
