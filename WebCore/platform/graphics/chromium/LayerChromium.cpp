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

#include "LayerChromium.h"

#include "PlatformContextSkia.h"
#include "RenderLayerBacking.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

using namespace std;

PassRefPtr<LayerChromium> LayerChromium::create(LayerType type, GraphicsLayerChromium* owner)
{
    return adoptRef(new LayerChromium(type, owner));
}

LayerChromium::LayerChromium(LayerType type, GraphicsLayerChromium* owner)
    : m_needsDisplayOnBoundsChange(false)
    , m_owner(owner)
    , m_layerType(type)
    , m_superlayer(0)
    , m_borderWidth(0)
    , m_borderColor(0, 0, 0, 0)
    , m_backgroundColor(0, 0, 0, 0)
    , m_anchorPoint(0.5, 0.5)
    , m_anchorPointZ(0)
    , m_clearsContext(false)
    , m_doubleSided(true)
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
    , m_contentsDirty(false)
    , m_contents(0)
    , m_hasContext(false)
{
}

LayerChromium::~LayerChromium()
{
    // Our superlayer should be holding a reference to us so there should be no
    // way for us to be destroyed while we still have a superlayer.
    ASSERT(!superlayer());

    // Remove the superlayer reference from all sublayers.
    removeAllSublayers();
}

void LayerChromium::updateGraphicsContext()
{
    // If the layer doesn't draw anything (e.g. it's a container layer) then we
    // don't create a canvas / context for it. The root layer is a special
    // case as even if it's marked as a container layer it does actually have
    // content that it draws.
    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_owner->client());
    if (!drawsContent() && !(this == rootLayer())) {
        m_graphicsContext.clear();
        m_skiaContext.clear();
        m_canvas.clear();
        m_hasContext = false;
        return;
    }

#if PLATFORM(SKIA)
    // Create new canvas and context. OwnPtr takes care of freeing up
    // the old ones.
    m_canvas = new skia::PlatformCanvas(m_backingStoreSize.width(), m_backingStoreSize.height(), false);
    m_skiaContext = new PlatformContextSkia(m_canvas.get());

    // This is needed to get text to show up correctly. Without it,
    // GDI renders with zero alpha and the text becomes invisible.
    // Unfortunately, setting this to true disables cleartype.
    m_skiaContext->setDrawingToImageBuffer(true);

    m_graphicsContext = new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(m_skiaContext.get()));

    m_hasContext = true;
    m_contentsDirty = true;
#else
#error "Need to implement for your platform."
#endif

    return;
}

void LayerChromium::drawsContentUpdated()
{
    // Create a drawing context if the layer now draws content
    // or delete the existing context if the layer doesn't draw
    // content anymore.
    updateGraphicsContext();
}

void LayerChromium::updateContents()
{
    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_owner->client());

    if (backing && !backing->paintingGoesToWindow() && drawsContent())
        m_owner->paintGraphicsLayerContents(*m_graphicsContext, IntRect(0, 0, m_bounds.width(), m_bounds.height()));

    m_contentsDirty = false;
}

void LayerChromium::setContents(NativeImagePtr contents)
{
    // Check if the image has changed.
    if (m_contents == contents)
        return;
    m_contents = contents;
    m_contentsDirty = true;
}

void LayerChromium::setNeedsCommit()
{
    // Call notifySyncRequired(), which in this implementation plumbs through to
    // call setRootLayerNeedsDisplay() on the WebView, which will cause LayerRendererChromium
    // to render a frame.
    if (m_owner)
        m_owner->notifySyncRequired();
}

void LayerChromium::addSublayer(PassRefPtr<LayerChromium> sublayer)
{
    insertSublayer(sublayer, numSublayers());
}

void LayerChromium::insertSublayer(PassRefPtr<LayerChromium> sublayer, size_t index)
{
    index = min(index, m_sublayers.size());
    sublayer->removeFromSuperlayer();
    sublayer->setSuperlayer(this);
    m_sublayers.insert(index, sublayer);
    setNeedsCommit();
}

void LayerChromium::removeFromSuperlayer()
{
    if (m_superlayer)
        m_superlayer->removeSublayer(this);
}

void LayerChromium::removeSublayer(LayerChromium* sublayer)
{
    int foundIndex = indexOfSublayer(sublayer);
    if (foundIndex == -1)
        return;

    sublayer->setSuperlayer(0);
    m_sublayers.remove(foundIndex);
    setNeedsCommit();
}

void LayerChromium::replaceSublayer(LayerChromium* reference, PassRefPtr<LayerChromium> newLayer)
{
    ASSERT_ARG(reference, reference);
    ASSERT_ARG(reference, reference->superlayer() == this);

    if (reference == newLayer)
        return;

    int referenceIndex = indexOfSublayer(reference);
    if (referenceIndex == -1) {
        ASSERT_NOT_REACHED();
        return;
    }

    reference->removeFromSuperlayer();

    if (newLayer) {
        newLayer->removeFromSuperlayer();
        insertSublayer(newLayer, referenceIndex);
    }
}

int LayerChromium::indexOfSublayer(const LayerChromium* reference)
{
    for (size_t i = 0; i < m_sublayers.size(); i++) {
        if (m_sublayers[i] == reference)
            return i;
    }
    return -1;
}

// This method can be called to overide the size of the backing store
// used for the layer. It's typically called on the root layer to limit
// its size to the actual visible size.
void LayerChromium::setBackingStoreSize(const IntSize& size)
{
    if (m_backingStoreSize == size)
        return;

    m_backingStoreSize = size;
    updateGraphicsContext();
    setNeedsCommit();
}

void LayerChromium::setBounds(const IntSize& size)
{
    if (m_bounds == size)
        return;

    m_bounds = size;
    m_backingStoreSize = size;

    // Re-create the canvas and associated contexts.
    updateGraphicsContext();
    setNeedsCommit();
}

void LayerChromium::setFrame(const FloatRect& rect)
{
    if (rect == m_frame)
      return;

    m_frame = rect;
    setNeedsCommit();
}

const LayerChromium* LayerChromium::rootLayer() const
{
    const LayerChromium* layer = this;
    for (LayerChromium* superlayer = layer->superlayer(); superlayer; layer = superlayer, superlayer = superlayer->superlayer()) { }
    return layer;
}

void LayerChromium::removeAllSublayers()
{
    while (m_sublayers.size()) {
        LayerChromium* layer = m_sublayers[0].get();
        ASSERT(layer->superlayer());
        layer->removeFromSuperlayer();
    }
    setNeedsCommit();
}

void LayerChromium::setSublayers(const Vector<RefPtr<LayerChromium> >& sublayers)
{
    if (sublayers == m_sublayers)
        return;

    removeAllSublayers();
    size_t listSize = sublayers.size();
    for (size_t i = 0; i < listSize; i++)
        addSublayer(sublayers[i]);
}

LayerChromium* LayerChromium::superlayer() const
{
    return m_superlayer;
}

void LayerChromium::setNeedsDisplay(const FloatRect& dirtyRect)
{
    // Simply mark the contents as dirty. The actual redraw will
    // happen when it's time to do the compositing.
    // FIXME: Should only update the dirty rect instead of marking
    //        the entire layer dirty.
    m_contentsDirty = true;

    setNeedsCommit();
}

void LayerChromium::setNeedsDisplay()
{
    // FIXME: implement
}

}

#endif // USE(ACCELERATED_COMPOSITING)
