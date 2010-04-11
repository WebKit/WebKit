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
#include "LayerRendererChromium.h"

#include "LayerChromium.h"
#include "PlatformContextSkia.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

PassOwnPtr<LayerRendererChromium> LayerRendererChromium::create()
{
    return new LayerRendererChromium();
}

LayerRendererChromium::LayerRendererChromium()
    : m_rootLayer(0)
    , m_needsDisplay(false)
{
}

LayerRendererChromium::~LayerRendererChromium()
{
}

void LayerRendererChromium::updateLayerContents()
{
    if (m_rootLayer)
        updateLayerContentsRecursive(m_rootLayer.get());
}

#if PLATFORM(SKIA)
void LayerRendererChromium::drawLayersInCanvas(skia::PlatformCanvas* canvas, const IntRect& clipRect)
{
    if (!m_rootLayer)
        return;

    canvas->save();
    canvas->clipRect(SkRect(clipRect));

    // First composite the root layer into the canvas.
    canvas->drawBitmap(m_rootLayer->platformCanvas()->getDevice()->accessBitmap(false), 0, 0, 0);

    // Account for the scroll offset before compositing the remaining layers.
    // Note that the root layer's painting takes into account the scroll offset already.
    canvas->translate(-m_scrollFrame.fLeft, -m_scrollFrame.fTop);

    float opacity = 1.0f;
    const Vector<RefPtr<LayerChromium> >& sublayers = m_rootLayer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        drawLayerInCanvasRecursive(canvas, sublayers[i].get(), opacity);

    canvas->restore();

    m_needsDisplay = false;
}

void LayerRendererChromium::drawLayerInCanvasRecursive(skia::PlatformCanvas* canvas, LayerChromium* layer, float opacity)
{
    // Guarantees that the canvas is restored to a known state on destruction.
    SkAutoCanvasRestore autoRestoreCanvas(canvas, true);

    FloatPoint position = layer->position();
    FloatPoint anchorPoint = layer->anchorPoint();
    SkMatrix transform = layer->transform().toAffineTransform();
    IntSize bounds = layer->bounds();

    canvas->translate(position.x(), position.y());

    SkScalar tx = SkScalarMul(anchorPoint.x(), bounds.width());
    SkScalar ty = SkScalarMul(anchorPoint.y(), bounds.height());
    canvas->translate(tx, ty);
    canvas->concat(transform);
    canvas->translate(-tx, -ty);

    // The position we get is for the center of the layer, but
    // drawBitmap starts at the upper-left corner, and therefore
    // we need to adjust our transform.
    canvas->translate(-0.5f * bounds.width(), -0.5f * bounds.height());

    layer->drawDebugBorder();

    SkPaint opacityPaint;
    opacity *= layer->opacity();
    opacityPaint.setAlpha(opacity * 255);

    canvas->drawBitmap(layer->platformCanvas()->getDevice()->accessBitmap(false), 0, 0, &opacityPaint);

    const Vector<RefPtr<LayerChromium> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        drawLayerInCanvasRecursive(canvas, sublayers[i].get(), opacity);
}
#endif // PLATFORM(SKIA)

void LayerRendererChromium::updateLayerContentsRecursive(LayerChromium* layer)
{
    layer->updateContents();

    const Vector<RefPtr<LayerChromium> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        updateLayerContentsRecursive(sublayers[i].get());
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
