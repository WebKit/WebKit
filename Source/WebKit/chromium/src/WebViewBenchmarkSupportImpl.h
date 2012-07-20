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

#ifndef WebViewBenchmarkSupportImpl_h
#define WebViewBenchmarkSupportImpl_h

#include "WebViewBenchmarkSupport.h"

namespace WebCore {
class IntRect;
class GraphicsLayer;
}

namespace WebKit {
class WebViewImpl;

class WebViewBenchmarkSupportImpl : public WebViewBenchmarkSupport {
public:
    explicit WebViewBenchmarkSupportImpl(WebViewImpl* webViewImpl) : m_webViewImpl(webViewImpl) { }
    virtual ~WebViewBenchmarkSupportImpl() { }

    // Paints the WebViewImpl onto canvases created from the PaintClient.
    // If we're in accelerated mode, a new canvas is created for each graphics
    // layer. Layout() will be called on the web view prior to painting.
    virtual void paint(PaintClient*, PaintMode) OVERRIDE;

private:
    void paintLayer(PaintClient*, WebCore::GraphicsLayer&, const WebCore::IntRect& clip);

    // Paints the given graphics layer hierarchy to canvases created by the
    // canvas factory.
    void acceleratedPaintUnclipped(PaintClient*, WebCore::GraphicsLayer&);
    void softwarePaint(PaintClient*, PaintMode);

    WebViewImpl* m_webViewImpl;
};
}

#endif // WebViewBenchmarkSupportImpl_h
