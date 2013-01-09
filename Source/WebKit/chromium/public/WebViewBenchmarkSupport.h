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
 *
 */

#ifndef WebViewBenchmarkSupport_h
#define WebViewBenchmarkSupport_h

#include <public/WebCanvas.h>
#include <public/WebSize.h>

namespace WebKit {

// Support for benchmarks accessing the WebView.
class WebViewBenchmarkSupport {
public:
    enum PaintMode {
        // Paint the entire page.
        PaintModeEverything
    };

    // Client for creating canvases where multiple canvases
    // may be used for layered rendering and sizes defined by the benchmark.
    // Also contains reporting methods called by the WebViewBenchmarkSupport
    // when painting is about to occur and when painting is complete.
    class PaintClient {
    public:
        // Called by the WebViewBenchmarkSupport when painting is about to occur.
        // PaintClient is expected to return an appropriately-sized canvas
        // for the WebViewBenchmarkSupport to paint on.
        virtual WebCanvas* willPaint(const WebSize&) { return 0; }

        // Called by the WebViewBenchmarkSupport when painting is complete.
        // The canvas will not be used after this call and can be destroyed
        // if necessary.
        virtual void didPaint(WebCanvas*) { }
    protected:
        virtual ~PaintClient() { }
    };

    // Paints the web view on canvases created from the client, using the given
    // paint mode.
    virtual void paint(PaintClient*, PaintMode) = 0;

protected:
    virtual ~WebViewBenchmarkSupport() { }
};
} // namespace WebKit

#endif // WebViewBenchmarkSupport_h
