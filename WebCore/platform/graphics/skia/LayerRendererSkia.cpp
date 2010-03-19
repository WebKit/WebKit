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


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "LayerRendererSkia.h"

#include "LayerSkia.h"
#include "PlatformContextSkia.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

PassOwnPtr<LayerRendererSkia> LayerRendererSkia::create()
{
    return new LayerRendererSkia();
}

LayerRendererSkia::LayerRendererSkia()
    : m_rootLayer(0)
    , m_needsDisplay(false)
{
}

LayerRendererSkia::~LayerRendererSkia()
{
}

void LayerRendererSkia::drawLayersInCanvas(skia::PlatformCanvas* canvas, SkRect clipRect)
{
    if (!m_rootLayer)
        return;

    canvas->save();
    canvas->clipRect(clipRect);

    // First composite the root layer into the canvas.
    canvas->drawBitmap(m_rootLayer->platformCanvas()->getDevice()->accessBitmap(false), 0, 0, 0);

    // Account for the scroll offset before compositing the remaining layers.
    // Note that the root layer's painting takes into account the scroll offset already.
    canvas->translate(-m_scrollFrame.fLeft, -m_scrollFrame.fTop);

    float opacity = 1.0f;
    const Vector<RefPtr<LayerSkia> >& sublayers = m_rootLayer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        drawLayerInCanvasRecursive(canvas, sublayers[i].get(), opacity);

    canvas->restore();

    m_needsDisplay = false;
}

void LayerRendererSkia::updateLayerContents()
{
    if (m_rootLayer)
        updateLayerContentsRecursive(m_rootLayer.get());
}

void LayerRendererSkia::drawLayerInCanvasRecursive(skia::PlatformCanvas* canvas, LayerSkia* layer, float opacity)
{
    // Guarantees that the canvas is restored to a known state on destruction.
    SkAutoCanvasRestore autoRestoreCanvas(canvas, true);

    SkPoint position = layer->position();
    SkPoint anchorPoint = layer->anchorPoint();
    SkMatrix transform = layer->transform();
    SkIRect bounds = layer->bounds();

    canvas->translate(position.fX, position.fY);

    SkScalar tx = SkScalarMul(anchorPoint.fX, bounds.width());
    SkScalar ty = SkScalarMul(anchorPoint.fY, bounds.height());
    canvas->translate(tx, ty);
    canvas->concat(transform);
    canvas->translate(-tx, -ty);

    // The position we get is for the center of the layer, but
    // drawBitmap starts at the upper-left corner, and therefore
    // we need to adjust our transform.
    canvas->translate(-0.5f * bounds.width(), -0.5f * bounds.height());

    SkPaint opacityPaint;
    opacity *= layer->opacity();
    opacityPaint.setAlpha(opacity * 255);

    canvas->drawBitmap(layer->platformCanvas()->getDevice()->accessBitmap(false), 0, 0, &opacityPaint);

    const Vector<RefPtr<LayerSkia> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        drawLayerInCanvasRecursive(canvas, sublayers[i].get(), opacity);
}

void LayerRendererSkia::updateLayerContentsRecursive(LayerSkia* layer)
{
    layer->updateContents();

    const Vector<RefPtr<LayerSkia> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        updateLayerContentsRecursive(sublayers[i].get());
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
