/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayerTextureUpdater.h"

#include "GraphicsContext.h"
#include "LayerPainterChromium.h"
#include "TraceEvent.h"

namespace WebCore {

CanvasLayerTextureUpdater::CanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : m_painter(painter)
{
}

CanvasLayerTextureUpdater::~CanvasLayerTextureUpdater()
{
}

void CanvasLayerTextureUpdater::paintContents(GraphicsContext& context, const IntRect& contentRect, float contentsScale)
{
    context.translate(-contentRect.x(), -contentRect.y());
    {
        TRACE_EVENT("LayerTextureUpdaterCanvas::paint", this, 0);

        IntRect scaledContentRect = contentRect;

        if (contentsScale != 1.0) {
            context.save();
            context.scale(FloatSize(contentsScale, contentsScale));

            FloatRect rect = contentRect;
            rect.scale(1 / contentsScale);
            scaledContentRect = enclosingIntRect(rect);
        }

        m_painter->paint(context, scaledContentRect);

        if (contentsScale != 1.0)
            context.restore();
    }
    m_contentRect = contentRect;
}

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
