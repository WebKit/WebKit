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

#include "FloatRect.h"
#include "LayerPainterChromium.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkRect.h"
#include "SkiaUtils.h"
#include "TraceEvent.h"

namespace WebCore {

CanvasLayerTextureUpdater::CanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : m_painter(painter)
{
}

CanvasLayerTextureUpdater::~CanvasLayerTextureUpdater()
{
}

void CanvasLayerTextureUpdater::paintContents(SkCanvas* canvas, const IntRect& contentRect, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect)
{
    TRACE_EVENT0("cc", "CanvasLayerTextureUpdater::paintContents");
    canvas->save();
    canvas->translate(WebCoreFloatToSkScalar(-contentRect.x()), WebCoreFloatToSkScalar(-contentRect.y()));

    IntRect layerRect = contentRect;

    if (contentsWidthScale != 1 || contentsHeightScale != 1) {
        canvas->scale(WebCoreFloatToSkScalar(contentsWidthScale), WebCoreFloatToSkScalar(contentsHeightScale));

        FloatRect rect = contentRect;
        rect.scale(1 / contentsWidthScale, 1 / contentsHeightScale);
        layerRect = enclosingIntRect(rect);
    }

    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    canvas->drawRect(layerRect, paint);
    canvas->clipRect(layerRect);

    FloatRect opaqueLayerRect;
    m_painter->paint(canvas, layerRect, opaqueLayerRect);
    canvas->restore();

    opaqueLayerRect.scale(contentsWidthScale, contentsHeightScale);
    resultingOpaqueRect = enclosedIntRect(opaqueLayerRect);

    m_contentRect = contentRect;
}

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
