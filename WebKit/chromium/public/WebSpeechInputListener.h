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

#ifndef WebSpeechInputListener_h
#define WebSpeechInputListener_h

#include "WebSpeechInputResult.h"

namespace WebKit {

class WebString;

// Provides a WebKit API called by the embedder.
// A typical sequence of calls to the listener would be
//   1 call to didCompleteRecording
//   0 or more calls to setRecognitionResult
//   1 call to didCompleteRecognition
class WebSpeechInputListener {
public:
    // Informs that audio recording has completed and recognition is underway. This gets invoked
    // irrespective of whether recording was stopped automatically by the 'endpointer' or if
    // WebSpeechInputController::stopRecording() was called.
    // Typically after this call the listener would update the UI to reflect that recognition is
    // in progress.
    virtual void didCompleteRecording(int) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Gives results from speech recognition, either partial or the final results.
    // This method can potentially get called multiple times if there are partial results
    // available as the user keeps speaking. If the speech could not be recognized properly
    // or if there was any other errors in the process, this method may never be called.
    virtual void setRecognitionResult(int, const WebSpeechInputResultArray&) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Informs that speech recognition has completed. This gets invoked irrespective of whether
    // recognition was succesful or not, whether setRecognitionResult() was invoked or not. The
    // handler typically frees up any temporary resources allocated and waits for the next speech
    // recognition request.
    virtual void didCompleteRecognition(int) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    ~WebSpeechInputListener() { }
};

} // namespace WebKit

#endif // WebSpeechInputListener_h
