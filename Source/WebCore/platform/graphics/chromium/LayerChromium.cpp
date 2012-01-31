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
#include "Region.h"
#include "RenderLayerBacking.h"
#include "TextStream.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

using namespace std;

static int s_nextLayerId = 1;

PassRefPtr<LayerChromium> LayerChromium::create()
{
    return adoptRef(new LayerChromium());
}

LayerChromium::LayerChromium()
    : m_needsDisplay(false)
    , m_layerId(s_nextLayerId++)
    , m_parent(0)
    , m_scrollable(false)
    , m_anchorPoint(0.5, 0.5)
    , m_backgroundColor(0, 0, 0, 0)
    , m_backgroundCoversViewport(false)
    , m_debugBorderWidth(0)
    , m_opacity(1.0)
    , m_anchorPointZ(0)
    , m_isDrawable(false)
    , m_masksToBounds(false)
    , m_opaque(false)
    , m_doubleSided(true)
    , m_usesLayerClipping(false)
    , m_isNonCompositedContent(false)
    , m_preserves3D(false)
    , m_alwaysReserveTextures(false)
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

void LayerChromium::setIsNonCompositedContent(bool isNonCompositedContent)
{
    m_isNonCompositedContent = isNonCompositedContent;
}

void LayerChromium::setLayerTreeHost(CCLayerTreeHost* host)
{
    if (m_layerTreeHost == host)
        return;

    // If we're changing hosts then we need to free up any resources
    // allocated by the old host.
    if (m_layerTreeHost)
        cleanupResources();

    m_layerTreeHost = host;

    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->setLayerTreeHost(host);

    if (m_maskLayer)
        m_maskLayer->setLayerTreeHost(host);
    if (m_replicaLayer)
        m_replicaLayer->setLayerTreeHost(host);
}

void LayerChromium::setNeedsCommit()
{
    if (m_layerTreeHost)
        m_layerTreeHost->setNeedsCommit();
}

void LayerChromium::setParent(LayerChromium* layer)
{
    ASSERT(!layer || !layer->hasAncestor(this));
    m_parent = layer;
    setLayerTreeHost(m_parent ? m_parent->layerTreeHost() : 0);
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
        setNeedsDisplay();
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

void LayerChromium::setAnchorPoint(const FloatPoint& anchorPoint)
{
    if (m_anchorPoint == anchorPoint)
        return;
    m_anchorPoint = anchorPoint;
    setNeedsCommit();
}

void LayerChromium::setAnchorPointZ(float anchorPointZ)
{
    if (m_anchorPointZ == anchorPointZ)
        return;
    m_anchorPointZ = anchorPointZ;
    setNeedsCommit();
}

void LayerChromium::setBackgroundColor(const Color& backgroundColor)
{
    if (m_backgroundColor == backgroundColor)
        return;
    m_backgroundColor = backgroundColor;
    setNeedsCommit();
}

void LayerChromium::setBackgroundCoversViewport(bool backgroundCoversViewport)
{
    if (m_backgroundCoversViewport == backgroundCoversViewport)
        return;
    m_backgroundCoversViewport = backgroundCoversViewport;
    setNeedsCommit();
}

void LayerChromium::setMasksToBounds(bool masksToBounds)
{
    if (m_masksToBounds == masksToBounds)
        return;
    m_masksToBounds = masksToBounds;
    setNeedsCommit();
}

void LayerChromium::setMaskLayer(LayerChromium* maskLayer)
{
    if (m_maskLayer == maskLayer)
        return;
    if (m_maskLayer)
        m_maskLayer->setLayerTreeHost(0);
    m_maskLayer = maskLayer;
    if (m_maskLayer)
        m_maskLayer->setLayerTreeHost(m_layerTreeHost.get());
    setNeedsCommit();
}

void LayerChromium::setReplicaLayer(LayerChromium* layer)
{
    if (m_replicaLayer == layer)
        return;
    if (m_replicaLayer)
        m_replicaLayer->setLayerTreeHost(0);
    m_replicaLayer = layer;
    if (m_replicaLayer)
        m_replicaLayer->setLayerTreeHost(m_layerTreeHost.get());
    setNeedsCommit();
}

void LayerChromium::setOpacity(float opacity)
{
    if (m_opacity == opacity)
        return;
    m_opacity = opacity;
    setNeedsCommit();
}

void LayerChromium::setOpaque(bool opaque)
{
    if (m_opaque == opaque)
        return;
    m_opaque = opaque;
    setNeedsDisplay();
}

void LayerChromium::setPosition(const FloatPoint& position)
{
    if (m_position == position)
        return;
    m_position = position;
    setNeedsCommit();
}

void LayerChromium::setSublayerTransform(const TransformationMatrix& sublayerTransform)
{
    if (m_sublayerTransform == sublayerTransform)
        return;
    m_sublayerTransform = sublayerTransform;
    setNeedsCommit();
}

void LayerChromium::setTransform(const TransformationMatrix& transform)
{
    if (m_transform == transform)
        return;
    m_transform = transform;
    setNeedsCommit();
}

void LayerChromium::setScrollPosition(const IntPoint& scrollPosition)
{
    if (m_scrollPosition == scrollPosition)
        return;
    m_scrollPosition = scrollPosition;
    setNeedsCommit();
}

void LayerChromium::setScrollable(bool scrollable)
{
    if (m_scrollable == scrollable)
        return;
    m_scrollable = scrollable;
    setNeedsCommit();
}

void LayerChromium::setDoubleSided(bool doubleSided)
{
    if (m_doubleSided == doubleSided)
        return;
    m_doubleSided = doubleSided;
    setNeedsCommit();
}

void LayerChromium::setIsDrawable(bool isDrawable)
{
    if (m_isDrawable == isDrawable)
        return;

    m_isDrawable = isDrawable;
    setNeedsCommit();
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
    layer->setBackgroundCoversViewport(m_backgroundCoversViewport);
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

    layer->setScrollDelta(layer->scrollDelta() - layer->sentScrollDelta());
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
    if (!needsContentsScale() || m_contentsScale == contentsScale)
        return;
    m_contentsScale = contentsScale;
    setNeedsDisplay();
}

TransformationMatrix LayerChromium::contentToScreenSpaceTransform() const
{
    IntSize boundsInLayerSpace = bounds();
    IntSize boundsInContentSpace = contentBounds();

    TransformationMatrix transform = screenSpaceTransform();

    // Scale from content space to layer space
    transform.scaleNonUniform(boundsInLayerSpace.width() / static_cast<double>(boundsInContentSpace.width()),
                              boundsInLayerSpace.height() / static_cast<double>(boundsInContentSpace.height()));

    return transform;
}

void LayerChromium::addSelfToOccludedScreenSpace(Region& occludedScreenSpace)
{
    if (!opaque() || drawOpacity() != 1 || !isPaintedAxisAlignedInScreen())
        return;

    FloatRect targetRect = contentToScreenSpaceTransform().mapRect(FloatRect(visibleLayerRect()));
    occludedScreenSpace.unite(enclosedIntRect(targetRect));
}

bool LayerChromium::isPaintedAxisAlignedInScreen() const
{
    FloatQuad quad = contentToScreenSpaceTransform().mapQuad(FloatQuad(visibleLayerRect()));
    return quad.isRectilinear();
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
