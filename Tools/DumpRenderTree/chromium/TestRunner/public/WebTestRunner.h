/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebTestRunner_h
#define WebTestRunner_h

namespace WebKit {
class WebArrayBufferView;
class WebPermissionClient;
}

namespace WebTestRunner {

class WebTestRunner {
public:
    // Returns a mock WebPermissionClient that is used for layout tests. An
    // embedder should use this for all WebViews it creates.
    virtual WebKit::WebPermissionClient* webPermissions() const = 0;

    // After WebTestDelegate::testFinished was invoked, the following methods
    // can be used to determine what kind of dump the main WebTestProxy can
    // provide.

    // If true, WebTestDelegate::audioData returns an audio dump and no text
    // or pixel results are available.
    virtual bool shouldDumpAsAudio() const = 0;
    virtual const WebKit::WebArrayBufferView* audioData() const = 0;

    // Returns true if the call to WebTestProxy::captureTree will invoke
    // WebTestDelegate::captureHistoryForWindow.
    virtual bool shouldDumpBackForwardList() const = 0;

    // Returns true if WebTestProxy::capturePixels should be invoked after
    // capturing text results.
    virtual bool shouldGeneratePixelResults() = 0;
};

}

#endif // WebTestRunner_h
