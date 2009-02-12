/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"

#include "AnimationController.h"
#include "ChromeClient.h"
#include "CSSPropertyNames.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Page.h"
#include "RenderLayerBacking.h"
#include "RenderView.h"

#if PROFILE_LAYER_REBUILD
#include <wtf/CurrentTime.h>
#endif

#ifndef NDEBUG
#include "CString.h"
#include "RenderTreeAsText.h"
#endif

namespace WebCore {

struct CompositingState {
    CompositingState(RenderLayer* compAncestor)
        : m_subtreeIsCompositing(false)
        , m_compositingAncestor(compAncestor)
#ifndef NDEBUG
        , m_depth(0)
#endif
    {
    }
    
    bool m_subtreeIsCompositing;
    RenderLayer* m_compositingAncestor;
#ifndef NDEBUG
    int m_depth;
#endif
};

static TransformationMatrix flipTransform()
{
    TransformationMatrix flipper;
    flipper.flipY();
    return flipper;
}

RenderLayerCompositor::RenderLayerCompositor(RenderView* renderView)
    : m_renderView(renderView)
    , m_rootPlatformLayer(0)
    , m_compositing(false)
    , m_rootLayerAttached(false)
    , m_compositingLayersNeedUpdate(false)
#if PROFILE_LAYER_REBUILD
    , m_rootLayerUpdateCount(0)
#endif // PROFILE_LAYER_REBUILD
{
}

RenderLayerCompositor::~RenderLayerCompositor()
{
    ASSERT(!m_rootLayerAttached);
    delete m_rootPlatformLayer;
}

void RenderLayerCompositor::enableCompositingMode(bool enable /* = true */)
{
    if (enable != m_compositing) {
        m_compositing = enable;
        
        // We never go out of compositing mode for a given page,
        // but if all the layers disappear, we'll just be left with
        // the empty root layer, which has minimal overhead.
        if (m_compositing)
            ensureRootPlatformLayer();
    }
}

void RenderLayerCompositor::setCompositingLayersNeedUpdate(bool needUpdate)
{
    if (inCompositingMode())
        m_compositingLayersNeedUpdate = needUpdate;
}

void RenderLayerCompositor::updateCompositingLayers(RenderLayer* updateRoot)
{
    if (!m_compositingLayersNeedUpdate)
        return;

    ASSERT(inCompositingMode());

    if (!updateRoot) {
        // Only clear the flag if we're updating the entire hierarchy
        m_compositingLayersNeedUpdate = false;
        updateRoot = rootRenderLayer();
    }

#if PROFILE_LAYER_REBUILD
    ++m_rootLayerUpdateCount;
    
    double startTime = WTF::currentTime();
#endif        

    // Go through the layers in presentation order, so that we can compute which
    // RLs need compositing layers.
    // FIXME: we could maybe do this in one pass, but the parenting logic would be more
    // complex.
    {
        CompositingState compState(updateRoot);
        computeCompositingRequirements(updateRoot, compState);
    }

    // Now create and parent the compositing layers.
    {
        CompositingState compState(updateRoot);
        rebuildCompositingLayerTree(updateRoot, compState);
    }
    
#if PROFILE_LAYER_REBUILD
    double endTime = WTF::currentTime();
    if (!updateRoot)
        fprintf(stderr, "Update %d: computeCompositingRequirements for the world took %fms\n"
                    m_rootLayerUpdateCount, 1000.0 * (endTime - startTime));
#endif
    ASSERT(updateRoot || !m_compositingLayersNeedUpdate);
}

bool RenderLayerCompositor::updateLayerCompositingState(RenderLayer* layer, StyleDifference diff)
{
    bool needsLayer = needsToBeComposited(layer);
    bool layerChanged = false;
    if (needsLayer) {
        enableCompositingMode();
        if (!layer->backing()) {
            layer->ensureBacking();
            layerChanged = true;
        }
    } else {
        if (layer->backing()) {
            layer->clearBacking();
            layerChanged = true;
        }
    }
    
    if (layerChanged) {
        // Invalidate the parent in this region.
        RenderLayer* compLayer = layer->ancestorCompositingLayer();
        if (compLayer) {
            // We can't reliably compute a dirty rect, because style may have changed already, 
            // so just dirty the whole parent layer
            compLayer->setBackingNeedsRepaint();
            // The contents of this layer may be moving between the window
            // and a GraphicsLayer, so we need to make sure the window system
            // synchronizes those changes on the screen.
            m_renderView->frameView()->setNeedsOneShotDrawingSynchronization();
        }
    }

    if (!needsLayer)
        return layerChanged;

    if (layer->backing()->updateGraphicsLayers(needsContentsCompositingLayer(layer),
                                               clippedByAncestor(layer),
                                               clipsCompositingDescendants(layer),
                                               diff >= StyleDifferenceRepaint))
        layerChanged = true;

    return layerChanged;
}

// The bounds of the GraphicsLayer created for a compositing layer is the union of the bounds of all the descendant
// RenderLayers that are rendered by the composited RenderLayer.
IntRect RenderLayerCompositor::calculateCompositedBounds(const RenderLayer* layer, const RenderLayer* ancestorLayer, IntRect* layerBoundingBox)
{
    IntRect boundingBoxRect, unionBounds;
    boundingBoxRect = unionBounds = layer->localBoundingBox();
    
    ASSERT(layer->isStackingContext() || (!layer->m_posZOrderList || layer->m_posZOrderList->size() == 0));

    Vector<RenderLayer*>* negZOrderList = layer->negZOrderList();
    if (negZOrderList) {
        for (Vector<RenderLayer*>::iterator it = negZOrderList->begin(); it != negZOrderList->end(); ++it) {
            RenderLayer* curLayer = (*it);
            if (!curLayer->isComposited()) {
                IntRect childUnionBounds = calculateCompositedBounds(curLayer, layer);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    Vector<RenderLayer*>* posZOrderList = layer->posZOrderList();
    if (posZOrderList) {
        for (Vector<RenderLayer*>::iterator it = posZOrderList->begin(); it != posZOrderList->end(); ++it) {
            RenderLayer* curLayer = (*it);
            if (!curLayer->isComposited()) {
                IntRect childUnionBounds = calculateCompositedBounds(curLayer, layer);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    Vector<RenderLayer*>* overflowList = layer->overflowList();
    if (overflowList) {
        for (Vector<RenderLayer*>::iterator it = overflowList->begin(); it != overflowList->end(); ++it) {
            RenderLayer* curLayer = (*it);
            if (!curLayer->isComposited()) {
                IntRect curAbsBounds = calculateCompositedBounds(curLayer, layer);
                unionBounds.unite(curAbsBounds);
            }
        }
    }

    if (!layer->isComposited() && layer->transform()) {
        TransformationMatrix* affineTrans = layer->transform();
        boundingBoxRect = affineTrans->mapRect(boundingBoxRect);
        unionBounds = affineTrans->mapRect(unionBounds);
    }

    int ancestorRelX = 0, ancestorRelY = 0;
    layer->convertToLayerCoords(ancestorLayer, ancestorRelX, ancestorRelY);
    unionBounds.move(ancestorRelX, ancestorRelY);

    if (layerBoundingBox) {
        boundingBoxRect.move(ancestorRelX, ancestorRelY);
        *layerBoundingBox = boundingBoxRect;
    }
    
    return unionBounds;
}

void RenderLayerCompositor::layerWasAdded(RenderLayer* /*parent*/, RenderLayer* /*child*/)
{
    setCompositingLayersNeedUpdate();
}

void RenderLayerCompositor::layerWillBeRemoved(RenderLayer* parent, RenderLayer* child)
{
    if (child->isComposited())
        setCompositingParent(child, 0);
    
    // If the document is being torn down (document's renderer() is null), then there's
    // no need to do any layer updating.
    if (parent->renderer()->documentBeingDestroyed())
        return;

    RenderLayer* compLayer = parent->enclosingCompositingLayer();
    if (compLayer) {
        IntRect ancestorRect = calculateCompositedBounds(child, compLayer);
        compLayer->setBackingNeedsRepaintInRect(ancestorRect);
        // The contents of this layer may be moving from a GraphicsLayer to the window,
        // so we need to make sure the window system synchronizes those changes on the screen.
        m_renderView->frameView()->setNeedsOneShotDrawingSynchronization();
    }

    setCompositingLayersNeedUpdate();
}

RenderLayer* RenderLayerCompositor::enclosingNonStackingClippingLayer(const RenderLayer* layer) const
{
    for (RenderLayer* curr = layer->parent(); curr != 0; curr = curr->parent()) {
        if (curr->isStackingContext())
            return 0;

        if (curr->renderer()->hasOverflowClip())
            return curr;
    }
    return 0;
}

//  Recurse through the layers in z-index and overflow order (which is equivalent to painting order)
//  For the z-order children of a compositing layer:
//      If a child layers has a compositing layer, then all subsequent layers must
//      be compositing in order to render above that layer.
//
//      If a child in the negative z-order list is compositing, then the layer itself
//      must be compositing so that its contents render over that child.
//      This implies that its positive z-index children must also be compositing.
//
void RenderLayerCompositor::computeCompositingRequirements(RenderLayer* layer, struct CompositingState& ioCompState)
{
    layer->updateLayerPosition();
    layer->updateZOrderLists();
    layer->updateOverflowList();
    
    // Clear the flag
    layer->setHasCompositingDescendant(false);

    setForcedCompositingLayer(layer, ioCompState.m_subtreeIsCompositing);
    
    const bool isCompositingLayer = needsToBeComposited(layer);
    ioCompState.m_subtreeIsCompositing = isCompositingLayer;

    CompositingState childState = ioCompState;
    if (isCompositingLayer)
        childState.m_compositingAncestor = layer;

    // The children of this stacking context don't need to composite, unless there is
    // a compositing layer among them, so start by assuming false.
    childState.m_subtreeIsCompositing = false;

#ifndef NDEBUG
    ++childState.m_depth;
#endif

    if (layer->isStackingContext()) {
        ASSERT(!layer->m_zOrderListsDirty);
        Vector<RenderLayer*>* negZOrderList = layer->negZOrderList();
        if (negZOrderList && negZOrderList->size() > 0) {
            for (Vector<RenderLayer*>::const_iterator it = negZOrderList->begin(); it != negZOrderList->end(); ++it) {
                RenderLayer* curLayer = (*it);
                computeCompositingRequirements(curLayer, childState);

                // if we have to make a layer for this child, make one now so we can have a contents layer
                // (since we need to ensure that the -ve z-order child renders underneath our contents)
                if (childState.m_subtreeIsCompositing) {
                    // make |this| compositing
                    setForcedCompositingLayer(layer, true);
                    childState.m_compositingAncestor = layer;
                }
            }
        }
    }
    
    ASSERT(!layer->m_overflowListDirty);
    Vector<RenderLayer*>* overflowList = layer->overflowList();
    if (overflowList && overflowList->size() > 0) {
        for (Vector<RenderLayer*>::const_iterator it = overflowList->begin(); it != overflowList->end(); ++it) {
            RenderLayer* curLayer = (*it);
            computeCompositingRequirements(curLayer, childState);
        }
    }

    if (layer->isStackingContext()) {
        Vector<RenderLayer*>* posZOrderList = layer->posZOrderList();
        if (posZOrderList && posZOrderList->size() > 0) {
            for (Vector<RenderLayer*>::const_iterator it = posZOrderList->begin(); it != posZOrderList->end(); ++it) {
                RenderLayer* curLayer = (*it);
                computeCompositingRequirements(curLayer, childState);
            }
        }
    }

    // If we have a software transform, and we have layers under us, we need to also
    // be composited. Also, if we have opacity < 1, then we need to be a layer so that
    // the child layers are opaque, then rendered with opacity on this layer.
    if (childState.m_subtreeIsCompositing &&
        (layer->renderer()->hasTransform() || layer->renderer()->style()->opacity() < 1))
        setForcedCompositingLayer(layer, true);

    // Subsequent layers in the parent stacking context also need to composite.
    if (childState.m_subtreeIsCompositing)
        ioCompState.m_subtreeIsCompositing = true;

    // Set the flag to say that this SC has compositing children.
    // this can affect the answer to needsToBeComposited() when clipping,
    // but that's ok here.
    layer->setHasCompositingDescendant(childState.m_subtreeIsCompositing);
}

void RenderLayerCompositor::setForcedCompositingLayer(RenderLayer* layer, bool force)
{
    if (force) {
        layer->ensureBacking();
        layer->backing()->forceCompositingLayer();
    } else {
        if (layer->backing())
            layer->backing()->forceCompositingLayer(false);
    }
}

void RenderLayerCompositor::setCompositingParent(RenderLayer* childLayer, RenderLayer* parentLayer)
{
    ASSERT(childLayer->isComposited());
    ASSERT(!parentLayer || parentLayer->isComposited());
    
    if (parentLayer) {
        GraphicsLayer* hostingLayer = parentLayer->backing()->parentForSublayers();
        GraphicsLayer* hostedLayer = childLayer->backing()->childForSuperlayers();
        
        hostingLayer->addChild(hostedLayer);
    } else
        childLayer->backing()->childForSuperlayers()->removeFromParent();
    
    // FIXME: setCompositingParent() is only called at present by rebuildCompositingLayerTree(),
    // which calls updateGraphicsLayerGeometry via updateLayerCompositingState(), so this should
    // be optimized.
    if (parentLayer)
        childLayer->backing()->updateGraphicsLayerGeometry();
}

void RenderLayerCompositor::removeCompositedChildren(RenderLayer* layer)
{
    ASSERT(layer->isComposited());

    GraphicsLayer* hostingLayer = layer->backing()->parentForSublayers();
    hostingLayer->removeAllChildren();
}

void RenderLayerCompositor::parentInRootLayer(RenderLayer* layer)
{
    ASSERT(layer->isComposited());

    GraphicsLayer* layerAnchor = layer->backing()->childForSuperlayers();

    if (layerAnchor->parent() != m_rootPlatformLayer) {
        layerAnchor->removeFromParent();
        if (m_rootPlatformLayer)
            m_rootPlatformLayer->addChild(layerAnchor);
    }
}

void RenderLayerCompositor::rebuildCompositingLayerTree(RenderLayer* layer, struct CompositingState& ioCompState)
{
    updateLayerCompositingState(layer, StyleDifferenceEqual);

    // host the document layer in the RenderView's root layer
    if (layer->isRootLayer())
        parentInRootLayer(layer);

    CompositingState childState = ioCompState;
    if (layer->isComposited())
        childState.m_compositingAncestor = layer;

#ifndef NDEBUG
    ++childState.m_depth;
#endif

    RenderLayerBacking* layerBacking = layer->backing();
    
    // FIXME: make this more incremental
    if (layer->isComposited()) {
        layerBacking->parentForSublayers()->removeAllChildren();
        layerBacking->updateInternalHierarchy();
    }
    
    // The children of this stacking context don't need to composite, unless there is
    // a compositing layer among them, so start by assuming false.
    childState.m_subtreeIsCompositing = false;

    if (layer->isStackingContext()) {
        ASSERT(!layer->m_zOrderListsDirty);

        Vector<RenderLayer*>* negZOrderList = layer->negZOrderList();
        if (negZOrderList && negZOrderList->size() > 0) {
            for (Vector<RenderLayer*>::const_iterator it = negZOrderList->begin(); it != negZOrderList->end(); ++it) {
                RenderLayer* curLayer = (*it);
                rebuildCompositingLayerTree(curLayer, childState);
                if (curLayer->isComposited())
                    setCompositingParent(curLayer, childState.m_compositingAncestor);
            }
        }

        if (layerBacking && layerBacking->contentsLayer()) {
            // we only have a contents layer if we have an m_layer
            layerBacking->contentsLayer()->removeFromParent();

            GraphicsLayer* hostingLayer = layerBacking->clippingLayer() ? layerBacking->clippingLayer() : layerBacking->graphicsLayer();
            hostingLayer->addChild(layerBacking->contentsLayer());
        }
    }

    ASSERT(!layer->m_overflowListDirty);
    Vector<RenderLayer*>* overflowList = layer->overflowList();
    if (overflowList && overflowList->size() > 0) {
        for (Vector<RenderLayer*>::iterator it = overflowList->begin(); it != overflowList->end(); ++it) {
            RenderLayer* curLayer = (*it);
            rebuildCompositingLayerTree(curLayer, childState);
            if (curLayer->isComposited())
                setCompositingParent(curLayer, childState.m_compositingAncestor);
        }
    }
    
    if (layer->isStackingContext()) {
        Vector<RenderLayer*>* posZOrderList = layer->posZOrderList();
        if (posZOrderList && posZOrderList->size() > 0) {
            for (Vector<RenderLayer*>::const_iterator it = posZOrderList->begin(); it != posZOrderList->end(); ++it) {
                RenderLayer* curLayer = (*it);
                rebuildCompositingLayerTree(curLayer, childState);
                if (curLayer->isComposited())
                    setCompositingParent(curLayer, childState.m_compositingAncestor);
            }
        }
    }
}

void RenderLayerCompositor::repaintCompositedLayersAbsoluteRect(const IntRect& absRect)
{
    recursiveRepaintLayerRect(rootRenderLayer(), absRect);
}

void RenderLayerCompositor::recursiveRepaintLayerRect(RenderLayer* layer, const IntRect& rect)
{
    if (layer->isComposited())
        layer->setBackingNeedsRepaintInRect(rect);

    if (layer->hasCompositingDescendant()) {
        Vector<RenderLayer*>* negZOrderList = layer->negZOrderList();
        if (negZOrderList) {
            for (Vector<RenderLayer*>::iterator it = negZOrderList->begin(); it != negZOrderList->end(); ++it) {
                RenderLayer* curLayer = (*it);
                int x = 0, y = 0;
                curLayer->convertToLayerCoords(layer, x, y);
                IntRect childRect(rect);
                childRect.move(-x, -y);
                recursiveRepaintLayerRect(curLayer, childRect);
            }
        }

        Vector<RenderLayer*>* posZOrderList = layer->posZOrderList();
        if (posZOrderList) {
            for (Vector<RenderLayer*>::iterator it = posZOrderList->begin(); it != posZOrderList->end(); ++it) {
                RenderLayer* curLayer = (*it);
                int x = 0, y = 0;
                curLayer->convertToLayerCoords(layer, x, y);
                IntRect childRect(rect);
                childRect.move(-x, -y);
                recursiveRepaintLayerRect(curLayer, childRect);
            }
        }
        
        Vector<RenderLayer*>* overflowList = layer->overflowList();
        if (overflowList) {
            for (Vector<RenderLayer*>::iterator it = overflowList->begin(); it != overflowList->end(); ++it) {
                RenderLayer* curLayer = (*it);
                int x = 0, y = 0;
                curLayer->convertToLayerCoords(layer, x, y);
                IntRect childRect(rect);
                childRect.move(-x, -y);
                recursiveRepaintLayerRect(curLayer, childRect);
            }
        }
    }
}

RenderLayer* RenderLayerCompositor::rootRenderLayer() const
{
    return m_renderView->layer();
}

GraphicsLayer* RenderLayerCompositor::rootPlatformLayer() const
{
    return m_rootPlatformLayer;
}

void RenderLayerCompositor::didMoveOnscreen()
{
    if (!m_rootPlatformLayer)
        return;

    Frame* frame = m_renderView->frameView()->frame();
    Page* page = frame ? frame->page() : 0;
    if (!page)
        return;

    page->chrome()->client()->attachRootGraphicsLayer(frame, m_rootPlatformLayer);
    m_rootLayerAttached = true;
}

void RenderLayerCompositor::willMoveOffscreen()
{
    if (!m_rootPlatformLayer || !m_rootLayerAttached)
        return;

    Frame* frame = m_renderView->frameView()->frame();
    Page* page = frame ? frame->page() : 0;
    if (!page)
        return;

    page->chrome()->client()->attachRootGraphicsLayer(frame, 0);
    m_rootLayerAttached = false;
}

void RenderLayerCompositor::updateRootLayerPosition()
{
    if (m_rootPlatformLayer)
        m_rootPlatformLayer->setSize(FloatSize(m_renderView->docWidth(), m_renderView->docHeight()));
}

bool RenderLayerCompositor::needsToBeComposited(const RenderLayer* layer) const
{
    return requiresCompositingLayer(layer) || (layer->backing() && layer->backing()->forcedCompositingLayer());
}

static bool requiresCompositingLayerForTransform(RenderObject*)
{
    // 2D transforms are rendered in software, unless animating (which is tested separately).
    return false;
}

#define VERBOSE_COMPOSITINGLAYER    0

// Note: this specifies whether the RL needs a compositing layer for intrinsic reasons.
// Use needsToBeComposited() to determine if a RL actually needs a compositing layer.
// static
bool RenderLayerCompositor::requiresCompositingLayer(const RenderLayer* layer) const
{
    // FIXME: cache the result of these tests?
#if VERBOSE_COMPOSITINGLAYER
    bool gotReason = false;

    if (!gotReason && inCompositingMode() && layer->isRootLayer()) {
        fprintf(stderr, "RenderLayer %p requires compositing layer because: it's the document root\n", layer);
        gotReason = true;
    }
    
    if (!gotReason && requiresCompositingLayerForTransform(layer->renderer())) {
        fprintf(stderr, "RenderLayer %p requires compositing layer because: it has 3d transform, perspective, backface, or animating transform\n", layer);
        gotReason = true;
    }

    if (!gotReason && clipsCompositingDescendants(layer)) {
        fprintf(stderr, "RenderLayer %p requires compositing layer because: it has overflow clip\n", layer);
        gotReason = true;
    }

    if (!gotReason && requiresCompositingForAnimation(layer)) {
        fprintf(stderr, "RenderLayer %p requires compositing layer because: it has a running transition for opacity or transform\n", layer);
        gotReason = true;
    }
    
    if (!gotReason)
        fprintf(stderr, "RenderLayer %p does not require compositing layer\n", layer);
#endif

    // The root layer always has a compositing layer, but it may not have backing.
    return (inCompositingMode() && layer->isRootLayer()) ||
             requiresCompositingLayerForTransform(layer->renderer()) ||
             clipsCompositingDescendants(layer) ||
             requiresCompositingForAnimation(layer);
}

// Return true if the given layer has some ancestor in the RenderLayer hierarchy that clips,
// up to the enclosing compositing ancestor. This is required because compositing layers are parented
// according to the z-order hierarchy, yet clipping goes down the renderer hierarchy.
// Thus, a RenderLayer can be clipped by a RenderLayer that is an ancestor in the renderer hierarchy,
// but a sibling in the z-order hierarchy.
bool RenderLayerCompositor::clippedByAncestor(RenderLayer* layer) const
{
    if (!layer->isComposited() || !layer->parent())
        return false;

    RenderLayer* compositingAncestor = layer->ancestorCompositingLayer();

    // We need ancestor clipping if something clips between this layer and its compositing, stacking context ancestor
    for (RenderLayer* curLayer = layer->parent(); curLayer && curLayer != compositingAncestor; curLayer = curLayer->parent()) {
        // FIXME: need to look at hasClip() too eventually
        if (curLayer->renderer()->hasOverflowClip())
            return true;
        
        // Clip is reset for an absolutely positioned element.
        // FIXME: many cases are broken. We need more of the logic in calculateClipRects() here
        if (curLayer->renderer()->style()->position() == AbsolutePosition)
            break;
    }

    return false;
}

// Return true if the given layer is a stacking context and has compositing child
// layers that it needs to clip. In this case we insert a clipping GraphicsLayer
// into the hierarchy between this layer and its children in the z-order hierarchy.
bool RenderLayerCompositor::clipsCompositingDescendants(const RenderLayer* layer) const
{
    // FIXME: need to look at hasClip() too eventually
    return layer->hasCompositingDescendant() &&
           layer->isStackingContext() &&
           layer->renderer()->hasOverflowClip();
}

bool RenderLayerCompositor::requiresCompositingForAnimation(const RenderLayer* layer) const
{
    AnimationController* animController = layer->renderer()->animation();
    if (animController)
        return animController->isAnimatingPropertyOnRenderer(layer->renderer(), CSSPropertyOpacity) ||
               animController->isAnimatingPropertyOnRenderer(layer->renderer(), CSSPropertyWebkitTransform);
    return false;
}

// If an element has negative z-index children, those children render in front of the 
// layer background, so we need an extra 'contents' layer for the foreground of the layer
// object.
bool RenderLayerCompositor::needsContentsCompositingLayer(const RenderLayer* layer) const
{
    return (layer->m_negZOrderList && layer->m_negZOrderList->size() > 0);
}

void RenderLayerCompositor::ensureRootPlatformLayer()
{
    if (m_rootPlatformLayer)
        return;

    m_rootPlatformLayer = GraphicsLayer::createGraphicsLayer(0);
    m_rootPlatformLayer->setSize(FloatSize(m_renderView->docWidth(), m_renderView->docHeight()));
    m_rootPlatformLayer->setPosition(FloatPoint(0, 0));

    if (GraphicsLayer::graphicsContextsFlipped())
        m_rootPlatformLayer->setChildrenTransform(flipTransform());

    // Need to clip to prevent transformed content showing outside this frame
    m_rootPlatformLayer->setMasksToBounds(true);
    
    didMoveOnscreen();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

