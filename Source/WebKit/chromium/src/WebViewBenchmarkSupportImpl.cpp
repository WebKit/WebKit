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
#include "config.h"
#include "WebViewBenchmarkSupportImpl.h"

#include "FloatSize.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "IntRect.h"
#include "IntSize.h"
#include "WebViewImpl.h"
#include "painting/GraphicsContextBuilder.h"

#include <public/WebCanvas.h>
#include <wtf/CurrentTime.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

void WebViewBenchmarkSupportImpl::paintLayer(PaintClient* paintClient, GraphicsLayer& layer, const IntRect& clip)
{
    WebSize canvasSize(clip.width(), clip.height());
    WebCanvas* canvas = paintClient->willPaint(canvasSize);
    GraphicsContextBuilder builder(canvas);

    layer.paintGraphicsLayerContents(builder.context(), clip);
    paintClient->didPaint(canvas);
}

void WebViewBenchmarkSupportImpl::acceleratedPaintUnclipped(PaintClient* paintClient, GraphicsLayer& layer)
{
    FloatSize layerSize = layer.size();
    IntRect clip(0, 0, layerSize.width(), layerSize.height());

    paintLayer(paintClient, layer, clip);

    const Vector<GraphicsLayer*>& children = layer.children();
    Vector<GraphicsLayer*>::const_iterator it;
    for (it = children.begin(); it != children.end(); ++it)
        acceleratedPaintUnclipped(paintClient, **it);
}

void WebViewBenchmarkSupportImpl::softwarePaint(PaintClient* paintClient, PaintMode paintMode)
{
    WebSize size = m_webViewImpl->size();
    WebRect paintSize;
    switch (paintMode) {
    case PaintModeEverything:
        if (m_webViewImpl->page() && m_webViewImpl->page()->mainFrame()) {
            FrameView* view = m_webViewImpl->page()->mainFrame()->view();
            IntSize contentsSize = view->contentsSize();
            paintSize = WebRect(0, 0, contentsSize.width(), contentsSize.height());
        } else
            paintSize = WebRect(0, 0, size.width, size.height);
        break;
    }

    WebSize canvasSize(paintSize.width, paintSize.height);
    WebCanvas* canvas = paintClient->willPaint(canvasSize);
    m_webViewImpl->paint(canvas, paintSize);
    paintClient->didPaint(canvas);
}

void WebViewBenchmarkSupportImpl::paint(PaintClient* paintClient, PaintMode paintMode)
{
    m_webViewImpl->layout();
    if (!m_webViewImpl->isAcceleratedCompositingActive())
        return softwarePaint(paintClient, paintMode);
    GraphicsLayer* layer = m_webViewImpl->rootGraphicsLayer();
    switch (paintMode) {
    case PaintModeEverything:
        acceleratedPaintUnclipped(paintClient, *layer);
        break;
    }
}
}
