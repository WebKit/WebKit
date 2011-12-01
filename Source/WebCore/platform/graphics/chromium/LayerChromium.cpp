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

#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHost.h"
#if USE(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#endif
#include "RenderLayerBacking.h"
#include "TextStream.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

using namespace std;

static int s_nextLayerId = 1;

PassRefPtr<LayerChromium> LayerChromium::create(CCLayerDelegate* delegate)
{
    return adoptRef(new LayerChromium(delegate));
}

LayerChromium::LayerChromium(CCLayerDelegate* delegate)
    : m_delegate(delegate)
    , m_needsDisplay(false)
    , m_layerId(s_nextLayerId++)
    , m_parent(0)
    , m_scrollable(false)
    , m_anchorPoint(0.5, 0.5)
    , m_backgroundColor(0, 0, 0, 0)
    , m_debugBorderWidth(0)
    , m_opacity(1.0)
    , m_anchorPointZ(0)
    , m_masksToBounds(false)
    , m_opaque(false)
    , m_doubleSided(true)
    , m_usesLayerClipping(false)
    , m_isNonCompositedContent(false)
    , m_preserves3D(false)
    , m_replicaLayer(0)
    , m_drawOpacity(0)
    , m_targetRenderSurface(0)
    , m_contentsScale(1.0)
    , m_pageScaleDirty(false)
{
}

LayerChromium::~LayerChromium()
{
    // Our parent should be holding a reference to us so there should be no
    // way for us to be destroyed while we still have a parent.
    ASSERT(!parent());

    // Remove the parent reference from all children.
    removeAllChildren();
}

void LayerChromium::cleanupResources()
{
}

void LayerChromium::cleanupResourcesRecursive()
{
    for (size_t i = 0; i < children().size(); ++i)
        children()[i]->cleanupResourcesRecursive();

    if (maskLayer())
        maskLayer()->cleanupResourcesRecursive();
    if (replicaLayer())
        replicaLayer()->cleanupResourcesRecursive();

    cleanupResources();
}

void LayerChromium::setLayerTreeHost(CCLayerTreeHost* host)
{
    // If we're changing layer renderers then we need to free up any resources
    // allocated by the old renderer.
    if (layerTreeHost() && layerTreeHost() != host) {
        cleanupResources();
        setNeedsDisplay();
    }

    m_layerTreeHost = host;
}

void LayerChromium::setNeedsCommit()
{
    // Call notifySyncRequired(), which for non-root layers plumbs through to
    // call setRootLayerNeedsDisplay() on the WebView, which will cause LayerRendererChromium
    // to render a frame.
    // This function has no effect on root layers.
    if (m_delegate)
        m_delegate->notifySyncRequired();
}

void LayerChromium::setParent(LayerChromium* layer)
{
    ASSERT(!layer || !layer->hasAncestor(this));
    m_parent = layer;
}

bool LayerChromium::hasAncestor(LayerChromium* ancestor) const
{
    for (LayerChromium* layer = parent(); layer; layer = layer->parent()) {
        if (layer == ancestor)
            return true;
    }
    return false;
}

void LayerChromium::addChild(PassRefPtr<LayerChromium> child)
{
    insertChild(child, numChildren());
}

void LayerChromium::insertChild(PassRefPtr<LayerChromium> child, size_t index)
{
    index = min(index, m_children.size());
    child->removeFromParent();
    child->setParent(this);
    m_children.insert(index, child);
    setNeedsCommit();
}

void LayerChromium::removeFromParent()
{
    if (m_parent)
        m_parent->removeChild(this);
}

void LayerChromium::removeChild(LayerChromium* child)
{
    int foundIndex = indexOfChild(child);
    if (foundIndex == -1)
        return;

    child->setParent(0);
    m_children.remove(foundIndex);
    setNeedsCommit();
}

void LayerChromium::replaceChild(LayerChromium* reference, PassRefPtr<LayerChromium> newLayer)
{
    ASSERT_ARG(reference, reference);
    ASSERT_ARG(reference, reference->parent() == this);

    if (reference == newLayer)
        return;

    int referenceIndex = indexOfChild(reference);
    if (referenceIndex == -1) {
        ASSERT_NOT_REACHED();
        return;
    }

    reference->removeFromParent();

    if (newLayer) {
        newLayer->removeFromParent();
        insertChild(newLayer, referenceIndex);
    }
}

int LayerChromium::indexOfChild(const LayerChromium* reference)
{
    for (size_t i = 0; i < m_children.size(); i++) {
        if (m_children[i] == reference)
            return i;
    }
    return -1;
}

void LayerChromium::setBounds(const IntSize& size)
{
    if (bounds() == size)
        return;

    bool firstResize = !bounds().width() && !bounds().height() && size.width() && size.height();

    m_bounds = size;

    if (firstResize || m_pageScaleDirty)
        setNeedsDisplayRect(FloatRect(0, 0, bounds().width(), bounds().height()));
    else
        setNeedsCommit();

    m_pageScaleDirty = false;
}

const LayerChromium* LayerChromium::rootLayer() const
{
    const LayerChromium* layer = this;
    for (LayerChromium* parent = layer->parent(); parent; layer = parent, parent = parent->parent()) { }
    return layer;
}

void LayerChromium::removeAllChildren()
{
    while (m_children.size()) {
        LayerChromium* layer = m_children[0].get();
        ASSERT(layer->parent());
        layer->removeFromParent();
    }
}

void LayerChromium::setChildren(const Vector<RefPtr<LayerChromium> >& children)
{
    if (children == m_children)
        return;

    removeAllChildren();
    size_t listSize = children.size();
    for (size_t i = 0; i < listSize; i++)
        addChild(children[i]);
}

LayerChromium* LayerChromium::parent() const
{
    return m_parent;
}

void LayerChromium::setName(const String& name)
{
    m_name = name;
}

void LayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    // Simply mark the contents as dirty. For non-root layers, the call to
    // setNeedsCommit will schedule a fresh compositing pass.
    // For the root layer, setNeedsCommit has no effect.
    if (!dirtyRect.isEmpty())
        m_needsDisplay = true;

    setNeedsCommit();
}

void LayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    layer->setAnchorPoint(m_anchorPoint);
    layer->setAnchorPointZ(m_anchorPointZ);
    layer->setBackgroundColor(m_backgroundColor);
    layer->setBounds(m_bounds);
    layer->setContentBounds(contentBounds());
    layer->setDebugBorderColor(m_debugBorderColor);
    layer->setDebugBorderWidth(m_debugBorderWidth);
    layer->setDoubleSided(m_doubleSided);
    layer->setDrawsContent(drawsContent());
    layer->setIsNonCompositedContent(m_isNonCompositedContent);
    layer->setMasksToBounds(m_masksToBounds);
    layer->setScrollable(m_scrollable);
    layer->setName(m_name);
    layer->setOpaque(m_opaque);
    layer->setOpacity(m_opacity);
    layer->setPosition(m_position);
    layer->setPreserves3D(preserves3D());
    layer->setScrollPosition(m_scrollPosition);
    layer->setSublayerTransform(m_sublayerTransform);
    layer->setTransform(m_transform);
    layer->setUpdateRect(m_updateRect);
    layer->setSentScrollDelta(IntSize());

    if (maskLayer())
        maskLayer()->pushPropertiesTo(layer->maskLayer());
    if (replicaLayer())
        replicaLayer()->pushPropertiesTo(layer->replicaLayer());

    // Reset any state that should be cleared for the next update.
    m_updateRect = FloatRect();
}

PassRefPtr<CCLayerImpl> LayerChromium::createCCLayerImpl()
{
    return CCLayerImpl::create(m_layerId);
}

void LayerChromium::setDebugBorderColor(const Color& color)
{
    m_debugBorderColor = color;
    setNeedsCommit();
}

void LayerChromium::setDebugBorderWidth(float width)
{
    m_debugBorderWidth = width;
    setNeedsCommit();
}

void LayerChromium::setContentsScale(float contentsScale)
{
    if (!needsContentsScale())
        return;
    m_contentsScale = contentsScale;
}

void LayerChromium::createRenderSurface()
{
    ASSERT(!m_renderSurface);
    m_renderSurface = adoptPtr(new RenderSurfaceChromium(this));
}

bool LayerChromium::descendantDrawsContent()
{
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i]->drawsContent() || m_children[i]->descendantDrawsContent())
            return true;
    }
    return false;
}

void sortLayers(Vector<RefPtr<LayerChromium> >::iterator, Vector<RefPtr<LayerChromium> >::iterator, void*)
{
    // Currently we don't use z-order to decide what to paint, so there's no need to actually sort LayerChromiums.
}

}
#endif // USE(ACCELERATED_COMPOSITING)
