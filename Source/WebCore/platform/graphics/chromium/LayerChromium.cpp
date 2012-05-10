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

#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "RenderLayerBacking.h"
#include "TextStream.h"
#include "cc/CCAnimationEvents.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCLayerAnimationDelegate.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHost.h"
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
    , m_stackingOrderChanged(false)
    , m_layerId(s_nextLayerId++)
    , m_parent(0)
    , m_layerTreeHost(0)
    , m_layerAnimationController(CCLayerAnimationController::create(this))
    , m_scrollable(false)
    , m_shouldScrollOnMainThread(false)
    , m_haveWheelEventHandlers(false)
    , m_nonFastScrollableRegionChanged(false)
    , m_anchorPoint(0.5, 0.5)
    , m_backgroundColor(0, 0, 0, 0)
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
    , m_drawCheckerboardForMissingTiles(false)
    , m_replicaLayer(0)
    , m_drawOpacity(0)
    , m_drawOpacityIsAnimating(false)
    , m_targetRenderSurface(0)
    , m_drawTransformIsAnimating(false)
    , m_screenSpaceTransformIsAnimating(false)
    , m_contentsScale(1.0)
    , m_layerAnimationDelegate(0)
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

void LayerChromium::setIsNonCompositedContent(bool isNonCompositedContent)
{
    m_isNonCompositedContent = isNonCompositedContent;
}

void LayerChromium::setLayerTreeHost(CCLayerTreeHost* host)
{
    if (m_layerTreeHost == host)
        return;

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
    child->m_stackingOrderChanged = true;
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

    if (firstResize)
        setNeedsDisplay();
    else
        setNeedsCommit();
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
    if (m_maskLayer) {
        m_maskLayer->setLayerTreeHost(m_layerTreeHost);
        m_maskLayer->setIsMask(true);
    }
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
        m_replicaLayer->setLayerTreeHost(m_layerTreeHost);
    setNeedsCommit();
}

void LayerChromium::setFilters(const FilterOperations& filters)
{
    if (m_filters == filters)
        return;
    m_filters = filters;
    setNeedsCommit();
    if (!filters.isEmpty())
        CCLayerTreeHost::setNeedsFilterContext(true);
}

void LayerChromium::setBackgroundFilters(const FilterOperations& backgroundFilters)
{
    if (m_backgroundFilters == backgroundFilters)
        return;
    m_backgroundFilters = backgroundFilters;
    setNeedsCommit();
}

void LayerChromium::setOpacity(float opacity)
{
    if (m_opacity == opacity)
        return;
    m_opacity = opacity;
    setNeedsCommit();
}

bool LayerChromium::opacityIsAnimating() const
{
    return m_layerAnimationController->isAnimatingProperty(CCActiveAnimation::Opacity);
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

bool LayerChromium::transformIsAnimating() const
{
    return m_layerAnimationController->isAnimatingProperty(CCActiveAnimation::Transform);
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

void LayerChromium::setShouldScrollOnMainThread(bool shouldScrollOnMainThread)
{
    if (m_shouldScrollOnMainThread == shouldScrollOnMainThread)
        return;
    m_shouldScrollOnMainThread = shouldScrollOnMainThread;
    setNeedsCommit();
}

void LayerChromium::setHaveWheelEventHandlers(bool haveWheelEventHandlers)
{
    if (m_haveWheelEventHandlers == haveWheelEventHandlers)
        return;
    m_haveWheelEventHandlers = haveWheelEventHandlers;
    setNeedsCommit();
}

void LayerChromium::setNonFastScrollableRegion(const Region& region)
{
    if (m_nonFastScrollableRegion == region)
        return;
    m_nonFastScrollableRegion = region;
    m_nonFastScrollableRegionChanged = true;
    setNeedsCommit();
}

void LayerChromium::setDrawCheckerboardForMissingTiles(bool checkerboard)
{
    if (m_drawCheckerboardForMissingTiles == checkerboard)
        return;
    m_drawCheckerboardForMissingTiles = checkerboard;
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

void LayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    m_updateRect.unite(dirtyRect);

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
    layer->setDebugName(m_debugName.isolatedCopy()); // We have to use isolatedCopy() here to safely pass ownership to another thread.
    layer->setDoubleSided(m_doubleSided);
    layer->setDrawCheckerboardForMissingTiles(m_drawCheckerboardForMissingTiles);
    layer->setDrawsContent(drawsContent());
    if (CCProxy::hasImplThread()) {
        // Since FilterOperations contains a vector of RefPtrs, we must deep copy the filters.
        FilterOperations filtersCopy;
        for (unsigned i = 0; i < m_filters.size(); ++i) {
            RefPtr<FilterOperation> clone = m_filters.at(i)->clone();
            if (clone)
                filtersCopy.operations().append(clone);
        }
        layer->setFilters(filtersCopy);
    } else
        layer->setFilters(filters());

    layer->setFilters(filters());
    layer->setBackgroundFilters(backgroundFilters());
    layer->setIsNonCompositedContent(m_isNonCompositedContent);
    layer->setMasksToBounds(m_masksToBounds);
    layer->setScrollable(m_scrollable);
    layer->setShouldScrollOnMainThread(m_shouldScrollOnMainThread);
    layer->setHaveWheelEventHandlers(m_haveWheelEventHandlers);
    // Copying a Region is more expensive than most layer properties, since it involves copying two Vectors that may be
    // arbitrarily large depending on page content, so we only push the property if it's changed.
    if (m_nonFastScrollableRegionChanged) {
        layer->setNonFastScrollableRegion(m_nonFastScrollableRegion);
        m_nonFastScrollableRegionChanged = false;
    }
    layer->setOpaque(m_opaque);
    if (!opacityIsAnimating())
        layer->setOpacity(m_opacity);
    layer->setPosition(m_position);
    layer->setPreserves3D(preserves3D());
    layer->setScrollPosition(m_scrollPosition);
    layer->setSublayerTransform(m_sublayerTransform);
    if (!transformIsAnimating())
        layer->setTransform(m_transform);

    // If the main thread commits multiple times before the impl thread actually draws, then damage tracking
    // will become incorrect if we simply clobber the updateRect here. The CCLayerImpl's updateRect needs to
    // accumulate (i.e. union) any update changes that have occurred on the main thread.
    m_updateRect.uniteIfNonZero(layer->updateRect());
    layer->setUpdateRect(m_updateRect);

    layer->setScrollDelta(layer->scrollDelta() - layer->sentScrollDelta());
    layer->setSentScrollDelta(IntSize());

    layer->setStackingOrderChanged(m_stackingOrderChanged);

    if (maskLayer())
        maskLayer()->pushPropertiesTo(layer->maskLayer());
    if (replicaLayer())
        replicaLayer()->pushPropertiesTo(layer->replicaLayer());

    m_layerAnimationController->pushAnimationUpdatesTo(layer->layerAnimationController());

    // Reset any state that should be cleared for the next update.
    m_stackingOrderChanged = false;
    m_updateRect = FloatRect();
}

PassOwnPtr<CCLayerImpl> LayerChromium::createCCLayerImpl()
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

void LayerChromium::setDebugName(const String& debugName)
{
    m_debugName = debugName;
    setNeedsCommit();
}


void LayerChromium::setContentsScale(float contentsScale)
{
    if (!needsContentsScale() || m_contentsScale == contentsScale)
        return;
    m_contentsScale = contentsScale;
    setNeedsDisplay();
}

void LayerChromium::createRenderSurface()
{
    ASSERT(!m_renderSurface);
    m_renderSurface = adoptPtr(new RenderSurfaceChromium(this));
    setTargetRenderSurface(m_renderSurface.get());
}

bool LayerChromium::descendantDrawsContent()
{
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i]->drawsContent() || m_children[i]->descendantDrawsContent())
            return true;
    }
    return false;
}

void LayerChromium::setOpacityFromAnimation(float opacity)
{
    // This is called due to an ongoing accelerated animation. Since this animation is
    // also being run on the impl thread, there is no need to request a commit to push
    // this value over, so set the value directly rather than calling setOpacity.
    m_opacity = opacity;
}

void LayerChromium::setTransformFromAnimation(const TransformationMatrix& transform)
{
    // This is called due to an ongoing accelerated animation. Since this animation is
    // also being run on the impl thread, there is no need to request a commit to push
    // this value over, so set this value directly rather than calling setTransform.
    m_transform = transform;
}

bool LayerChromium::addAnimation(const KeyframeValueList& values, const IntSize& boxSize, const Animation* animation, int animationId, int groupId, double timeOffset)
{
    if (!m_layerTreeHost || !m_layerTreeHost->settings().threadedAnimationEnabled)
        return false;

    bool addedAnimation = m_layerAnimationController->addAnimation(values, boxSize, animation, animationId, groupId, timeOffset);
    if (addedAnimation) {
        m_layerTreeHost->didAddAnimation();
        setNeedsCommit();
    }
    return addedAnimation;
}

void LayerChromium::pauseAnimation(int animationId, double timeOffset)
{
    m_layerAnimationController->pauseAnimation(animationId, timeOffset);
    setNeedsCommit();
}

void LayerChromium::removeAnimation(int animationId)
{
    m_layerAnimationController->removeAnimation(animationId);
    setNeedsCommit();
}

void LayerChromium::suspendAnimations(double monotonicTime)
{
    m_layerAnimationController->suspendAnimations(monotonicTime);
    setNeedsCommit();
}

void LayerChromium::resumeAnimations(double monotonicTime)
{
    m_layerAnimationController->resumeAnimations(monotonicTime);
    setNeedsCommit();
}

void LayerChromium::setLayerAnimationController(PassOwnPtr<CCLayerAnimationController> layerAnimationController)
{
    m_layerAnimationController = layerAnimationController;
    setNeedsCommit();
}

bool LayerChromium::hasActiveAnimation() const
{
    return m_layerAnimationController->hasActiveAnimation();
}

void LayerChromium::notifyAnimationStarted(const CCAnimationEvent& event, double wallClockTime)
{
    m_layerAnimationController->notifyAnimationStarted(event);
    m_layerAnimationDelegate->notifyAnimationStarted(wallClockTime);
}

void LayerChromium::notifyAnimationFinished(double wallClockTime)
{
    m_layerAnimationDelegate->notifyAnimationFinished(wallClockTime);
}

Region LayerChromium::visibleContentOpaqueRegion() const
{
    if (opaque())
        return visibleLayerRect();
    return Region();
}

void sortLayers(Vector<RefPtr<LayerChromium> >::iterator, Vector<RefPtr<LayerChromium> >::iterator, void*)
{
    // Currently we don't use z-order to decide what to paint, so there's no need to actually sort LayerChromiums.
}

}
#endif // USE(ACCELERATED_COMPOSITING)
