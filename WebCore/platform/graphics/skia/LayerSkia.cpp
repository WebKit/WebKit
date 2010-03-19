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

#include "LayerSkia.h"

#include "PlatformContextSkia.h"
#include "RenderLayerBacking.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

using namespace std;

PassRefPtr<LayerSkia> LayerSkia::create(LayerType type, GraphicsLayerSkia* owner)
{
    return adoptRef(new LayerSkia(type, owner));
}

LayerSkia::LayerSkia(LayerType type, GraphicsLayerSkia* owner)
    : m_needsDisplayOnBoundsChange(false)
    , m_owner(owner)
    , m_layerType(type)
    , m_superlayer(0)
    , m_borderWidth(0)
    , m_borderColor(0, 0, 0, 0)
    , m_backgroundColor(0, 0, 0, 0)
    , m_anchorPointZ(0)
    , m_clearsContext(false)
    , m_doubleSided(false)
    , m_edgeAntialiasingMask(0)
    , m_hidden(false)
    , m_masksToBounds(false)
    , m_contentsGravity(Bottom)
    , m_opacity(1.0)
    , m_opaque(true)
    , m_zPosition(0.0)
    , m_canvas(0)
    , m_skiaContext(0)
    , m_graphicsContext(0)
    , m_geometryFlipped(false)
{
    m_bounds.setEmpty();
    m_backingStoreRect.setEmpty();
    m_frame.setEmpty();
    m_position.set(0, 0);
    m_anchorPoint.set(0, 0);
    m_transform.reset();
    m_sublayerTransform.reset();

    updateGraphicsContext(m_backingStoreRect);
}

LayerSkia::~LayerSkia()
{
    // Our superlayer should be holding a reference to us, so there should be no way for us to be destroyed while we still have a superlayer.
    ASSERT(!superlayer());
}

void LayerSkia::updateGraphicsContext(const SkIRect& rect)
{
    // Create new canvas and context. OwnPtr takes care of freeing up
    // the old ones.
    m_canvas = new skia::PlatformCanvas(rect.width(), rect.height(), false);
    m_skiaContext = new PlatformContextSkia(m_canvas.get());

    // This is needed to get text to show up correctly. Without it,
    // GDI renders with zero alpha and the text becomes invisible.
    // Unfortunately, setting this to true disables cleartype.
    m_skiaContext->setDrawingToImageBuffer(true);

    m_graphicsContext = new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(m_skiaContext.get()));

    // The backing store allocated for a layer can be smaller than the layer's bounds.
    // This is mostly true for the root layer whose backing store is sized based on the visible
    // portion of the layer rather than the actual page size.
    m_backingStoreRect = rect;
}

void LayerSkia::updateContents()
{
    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_owner->client());

    if (backing && !backing->paintingGoesToWindow())
        m_owner->paintGraphicsLayerContents(*m_graphicsContext, IntRect(0, 0, m_bounds.width(), m_bounds.height()));

    // Paint the debug border.
    // FIXME: Add a flag to check if debug borders are used.
    m_graphicsContext->setStrokeColor(m_borderColor, DeviceColorSpace);
    m_graphicsContext->setStrokeThickness(m_borderWidth);
    m_graphicsContext->drawLine(IntPoint(0, 0), IntPoint(m_bounds.width(), 0));
    m_graphicsContext->drawLine(IntPoint(0, 0), IntPoint(0, m_bounds.height()));
    m_graphicsContext->drawLine(IntPoint(m_bounds.width(), 0), IntPoint(m_bounds.width(), m_bounds.height()));
    m_graphicsContext->drawLine(IntPoint(0, m_bounds.height()), IntPoint(m_bounds.width(), m_bounds.height()));
}

void LayerSkia::setNeedsCommit()
{
    // Call notifySyncRequired(), which in this implementation plumbs through to
    // call setRootLayerNeedsDisplay() on the WebView, which will cause LayerRendererSkia
    // to render a frame.
    if (m_owner)
        m_owner->notifySyncRequired();
}

void LayerSkia::addSublayer(PassRefPtr<LayerSkia> sublayer)
{
    insertSublayer(sublayer, numSublayers());
}

void LayerSkia::insertSublayer(PassRefPtr<LayerSkia> sublayer, size_t index)
{
    index = min(index, m_sublayers.size());
    m_sublayers.insert(index, sublayer);
    sublayer->setSuperlayer(this);
    setNeedsCommit();
}

void LayerSkia::removeFromSuperlayer()
{
    LayerSkia* superlayer = this->superlayer();
    if (!superlayer)
        return;

    superlayer->removeSublayer(this);
}

void LayerSkia::removeSublayer(LayerSkia* sublayer)
{
    int foundIndex = indexOfSublayer(sublayer);
    if (foundIndex == -1)
        return;

    m_sublayers.remove(foundIndex);
    sublayer->setSuperlayer(0);
    setNeedsCommit();
}

int LayerSkia::indexOfSublayer(const LayerSkia* reference)
{
    for (size_t i = 0; i < m_sublayers.size(); i++) {
        if (m_sublayers[i] == reference)
            return i;
    }
    return -1;
}

void LayerSkia::setBackingStoreRect(const SkIRect& rect)
{
    if (m_backingStoreRect == rect)
        return;

    updateGraphicsContext(rect);
}

void LayerSkia::setBounds(const SkIRect& rect)
{
    if (rect == m_bounds)
        return;

    m_bounds = rect;

    // Re-create the canvas and associated contexts.
    updateGraphicsContext(m_bounds);

    setNeedsCommit();
}

void LayerSkia::setFrame(const SkRect& rect)
{
    if (rect == m_frame)
      return;

    m_frame = rect;
    setNeedsCommit();
}

const LayerSkia* LayerSkia::rootLayer() const
{
    const LayerSkia* layer = this;
    for (LayerSkia* superlayer = layer->superlayer(); superlayer; layer = superlayer, superlayer = superlayer->superlayer()) { }
    return layer;
}

void LayerSkia::removeAllSublayers()
{
    m_sublayers.clear();
    setNeedsCommit();
}

void LayerSkia::setSublayers(const Vector<RefPtr<LayerSkia> >& sublayers)
{
    m_sublayers = sublayers;
}

void LayerSkia::setSuperlayer(LayerSkia* superlayer)
{
    m_superlayer = superlayer;
}

LayerSkia* LayerSkia::superlayer() const
{
    return m_superlayer;
}

void LayerSkia::setNeedsDisplay(const SkRect& dirtyRect)
{
    // FIXME: implement
    setNeedsCommit();
}

void LayerSkia::setNeedsDisplay()
{
    // FIXME: implement
}

}

#endif // USE(ACCELERATED_COMPOSITING)
