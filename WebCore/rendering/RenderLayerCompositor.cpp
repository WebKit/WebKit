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
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Page.h"
#include "RenderLayerBacking.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "Settings.h"

#if PROFILE_LAYER_REBUILD
#include <wtf/CurrentTime.h>
#endif

#ifndef NDEBUG
#include "CString.h"
#include "RenderTreeAsText.h"
#endif

#if ENABLE(3D_RENDERING)
// This symbol is used to determine from a script whether 3D rendering is enabled (via 'nm').
bool WebCoreHas3DRendering = true;
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
    , m_hasAcceleratedCompositing(true)
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
        else
            destroyRootPlatformLayer();
    }
}

void RenderLayerCompositor::cacheAcceleratedCompositingEnabledFlag()
{
    bool hasAcceleratedCompositing = false;
    if (Settings* settings = m_renderView->document()->settings())
        hasAcceleratedCompositing = settings-> acceleratedCompositingEnabled();

    if (hasAcceleratedCompositing != m_hasAcceleratedCompositing)
        setCompositingLayersNeedUpdate();
        
    m_hasAcceleratedCompositing = hasAcceleratedCompositing;
}

void RenderLayerCompositor::setCompositingLayersNeedUpdate(bool needUpdate)
{
    if (inCompositingMode()) {
        if (!m_compositingLayersNeedUpdate && needUpdate)
            scheduleViewUpdate();

        m_compositingLayersNeedUpdate = needUpdate;
    }
}

void RenderLayerCompositor::scheduleViewUpdate()
{
    Frame* frame = m_renderView->frameView()->frame();
    Page* page = frame ? frame->page() : 0;
    if (!page)
        return;

    page->chrome()->client()->scheduleViewUpdate();
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
    if (updateRoot == rootRenderLayer())
        fprintf(stderr, "Update %d: computeCompositingRequirements for the world took %fms\n",
                    m_rootLayerUpdateCount, 1000.0 * (endTime - startTime));
#endif
    ASSERT(updateRoot || !m_compositingLayersNeedUpdate);

    if (!hasAcceleratedCompositing())
        enableCompositingMode(false);
}

bool RenderLayerCompositor::updateBacking(RenderLayer* layer, CompositingChangeRepaint shouldRepaint)
{
    bool layerChanged = false;

    if (needsToBeComposited(layer)) {
        enableCompositingMode();
        if (!layer->backing()) {

            // If we need to repaint, do so before making backing
            if (shouldRepaint == CompositingChangeRepaintNow)
                repaintOnCompositingChange(layer);

            layer->ensureBacking();
            layerChanged = true;
        }
    } else {
        if (layer->backing()) {
            layer->clearBacking();
            layerChanged = true;

            // If we need to repaint, do so now that we've removed the backing
            if (shouldRepaint == CompositingChangeRepaintNow)
                repaintOnCompositingChange(layer);
        }
    }
    
#if ENABLE(VIDEO)
    if (layerChanged && layer->renderer()->isVideo()) {
        // If it's a video, give the media player a chance to hook up to the layer.
        RenderVideo* video = static_cast<RenderVideo*>(layer->renderer());
        video->acceleratedRenderingStateChanged();
    }
#endif
    return layerChanged;
}

bool RenderLayerCompositor::updateLayerCompositingState(RenderLayer* layer, CompositingChangeRepaint shouldRepaint)
{
    bool layerChanged = updateBacking(layer, shouldRepaint);

    // See if we need content or clipping layers. Methods called here should assume
    // that the compositing state of descendant layers has not been updated yet.
    if (layer->backing() && layer->backing()->updateGraphicsLayerConfiguration())
        layerChanged = true;

    return layerChanged;
}

void RenderLayerCompositor::repaintOnCompositingChange(RenderLayer* layer)
{
    RenderBoxModelObject* repaintContainer = layer->renderer()->containerForRepaint();
    if (!repaintContainer)
        repaintContainer = m_renderView;

    layer->repaintIncludingNonCompositingDescendants(repaintContainer);
    if (repaintContainer == m_renderView) {
        // The contents of this layer may be moving between the window
        // and a GraphicsLayer, so we need to make sure the window system
        // synchronizes those changes on the screen.
        m_renderView->frameView()->setNeedsOneShotDrawingSynchronization();
    }
}

// The bounds of the GraphicsLayer created for a compositing layer is the union of the bounds of all the descendant
// RenderLayers that are rendered by the composited RenderLayer.
IntRect RenderLayerCompositor::calculateCompositedBounds(const RenderLayer* layer, const RenderLayer* ancestorLayer)
{
    IntRect boundingBoxRect, unionBounds;
    boundingBoxRect = unionBounds = layer->localBoundingBox();
    
    ASSERT(layer->isStackingContext() || (!layer->m_posZOrderList || layer->m_posZOrderList->size() == 0));

    if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
        size_t listSize = negZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = negZOrderList->at(i);
            if (!curLayer->isComposited()) {
                IntRect childUnionBounds = calculateCompositedBounds(curLayer, layer);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
        size_t listSize = posZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = posZOrderList->at(i);
            if (!curLayer->isComposited()) {
                IntRect childUnionBounds = calculateCompositedBounds(curLayer, layer);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            if (!curLayer->isComposited()) {
                IntRect curAbsBounds = calculateCompositedBounds(curLayer, layer);
                unionBounds.unite(curAbsBounds);
            }
        }
    }

    if (layer->paintsWithTransform()) {
        TransformationMatrix* affineTrans = layer->transform();
        boundingBoxRect = affineTrans->mapRect(boundingBoxRect);
        unionBounds = affineTrans->mapRect(unionBounds);
    }

    int ancestorRelX = 0, ancestorRelY = 0;
    layer->convertToLayerCoords(ancestorLayer, ancestorRelX, ancestorRelY);
    unionBounds.move(ancestorRelX, ancestorRelY);

    return unionBounds;
}

void RenderLayerCompositor::layerWasAdded(RenderLayer* /*parent*/, RenderLayer* /*child*/)
{
    setCompositingLayersNeedUpdate();
}

void RenderLayerCompositor::layerWillBeRemoved(RenderLayer* parent, RenderLayer* child)
{
    if (!child->isComposited() || parent->renderer()->documentBeingDestroyed())
        return;

    setCompositingParent(child, 0);
    
    RenderLayer* compLayer = parent->enclosingCompositingLayer();
    if (compLayer) {
        ASSERT(compLayer->backing());
        IntRect compBounds = child->backing()->compositedBounds();

        int offsetX = 0, offsetY = 0;
        child->convertToLayerCoords(compLayer, offsetX, offsetY);
        compBounds.move(offsetX, offsetY);

        compLayer->setBackingNeedsRepaintInRect(compBounds);

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
void RenderLayerCompositor::computeCompositingRequirements(RenderLayer* layer, struct CompositingState& compositingState)
{
    layer->updateLayerPosition();
    layer->updateZOrderLists();
    layer->updateNormalFlowList();
    
    // Clear the flag
    layer->setHasCompositingDescendant(false);
    layer->setMustOverlayCompositedLayers(compositingState.m_subtreeIsCompositing);
    
    const bool willBeComposited = needsToBeComposited(layer);
    compositingState.m_subtreeIsCompositing = willBeComposited;

    CompositingState childState = compositingState;
    if (willBeComposited)
        childState.m_compositingAncestor = layer;

    // The children of this stacking context don't need to composite, unless there is
    // a compositing layer among them, so start by assuming false.
    childState.m_subtreeIsCompositing = false;

#if ENABLE(VIDEO)
    // Video is special. It's a replaced element with a content layer, but has shadow content
    // for the controller that must render in front. Without this, the controls fail to show
    // when the video element is a stacking context (e.g. due to opacity or transform).
    if (willBeComposited && layer->renderer()->isVideo())
        childState.m_subtreeIsCompositing = true;
#endif

#ifndef NDEBUG
    ++childState.m_depth;
#endif

    if (layer->isStackingContext()) {
        ASSERT(!layer->m_zOrderListsDirty);
        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                computeCompositingRequirements(curLayer, childState);

                // If we have to make a layer for this child, make one now so we can have a contents layer
                // (since we need to ensure that the -ve z-order child renders underneath our contents).
                if (childState.m_subtreeIsCompositing) {
                    // make layer compositing
                    layer->setMustOverlayCompositedLayers(true);
                    childState.m_compositingAncestor = layer;
                }
            }
        }
    }
    
    ASSERT(!layer->m_normalFlowListDirty);
    Vector<RenderLayer*>* normalFlowList = layer->normalFlowList();
    if (normalFlowList && normalFlowList->size() > 0) {
        for (Vector<RenderLayer*>::const_iterator it = normalFlowList->begin(); it != normalFlowList->end(); ++it) {
            RenderLayer* curLayer = (*it);
            computeCompositingRequirements(curLayer, childState);
        }
    }

    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                computeCompositingRequirements(curLayer, childState);
            }
        }
    }

    // If we have a software transform, and we have layers under us, we need to also
    // be composited. Also, if we have opacity < 1, then we need to be a layer so that
    // the child layers are opaque, then rendered with opacity on this layer.
    if (childState.m_subtreeIsCompositing &&
        (layer->renderer()->hasTransform() || layer->renderer()->style()->opacity() < 1))
        layer->setMustOverlayCompositedLayers(true);

    // Subsequent layers in the parent stacking context also need to composite.
    if (childState.m_subtreeIsCompositing)
        compositingState.m_subtreeIsCompositing = true;

    // If the layer is going into compositing mode, repaint its old location.
    if (!layer->isComposited() && needsToBeComposited(layer))
        repaintOnCompositingChange(layer);

    // Set the flag to say that this SC has compositing children.
    // this can affect the answer to needsToBeComposited() when clipping,
    // but that's ok here.
    layer->setHasCompositingDescendant(childState.m_subtreeIsCompositing);

    // Update backing now, so that we can use isComposited() reliably during tree traversal in rebuildCompositingLayerTree().
    updateBacking(layer, CompositingChangeRepaintNow);
}

void RenderLayerCompositor::setCompositingParent(RenderLayer* childLayer, RenderLayer* parentLayer)
{
    ASSERT(childLayer->isComposited());

    // It's possible to be called with a parent that isn't yet composited when we're doing
    // partial updates as required by painting or hit testing. Just bail in that case;
    // we'll do a full layer update soon.
    if (!parentLayer || !parentLayer->isComposited())
        return;

    if (parentLayer) {
        GraphicsLayer* hostingLayer = parentLayer->backing()->parentForSublayers();
        GraphicsLayer* hostedLayer = childLayer->backing()->childForSuperlayers();
        
        hostingLayer->addChild(hostedLayer);
    } else
        childLayer->backing()->childForSuperlayers()->removeFromParent();
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

#if ENABLE(VIDEO)
bool RenderLayerCompositor::canAccelerateVideoRendering(RenderVideo* o) const
{
    // FIXME: ideally we need to look at all ancestors for mask or video. But for now,
    // just bail on the obvious cases.
    if (o->hasMask() || o->hasReflection() || !m_hasAcceleratedCompositing)
        return false;

    return o->supportsAcceleratedRendering();
}
#endif

void RenderLayerCompositor::rebuildCompositingLayerTree(RenderLayer* layer, struct CompositingState& compositingState)
{
    // Make the layer compositing if necessary, and set up clipping and content layers.
    // Note that we can only do work here that is independent of whether the descendant layers
    // have been processed. computeCompositingRequirements() will already have done the repaint if necessary.
    RenderLayerBacking* layerBacking = layer->backing();
    if (layerBacking) {
        // The compositing state of all our children has been updated already, so now
        // we can compute and cache the composited bounds for this layer.
        layerBacking->setCompositedBounds(calculateCompositedBounds(layer, layer));

        layerBacking->updateGraphicsLayerConfiguration();
        layerBacking->updateGraphicsLayerGeometry();

        if (!layer->parent())
            updateRootLayerPosition();

        // FIXME: make this more incremental
        layerBacking->parentForSublayers()->removeAllChildren();
    }

    // host the document layer in the RenderView's root layer
    if (layer->isRootLayer() && layer->isComposited())
        parentInRootLayer(layer);

    CompositingState childState = compositingState;
    if (layer->isComposited())
        childState.m_compositingAncestor = layer;

#ifndef NDEBUG
    ++childState.m_depth;
#endif

    // The children of this stacking context don't need to composite, unless there is
    // a compositing layer among them, so start by assuming false.
    childState.m_subtreeIsCompositing = false;

    if (layer->isStackingContext()) {
        ASSERT(!layer->m_zOrderListsDirty);

        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
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

    ASSERT(!layer->m_normalFlowListDirty);
    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            rebuildCompositingLayerTree(curLayer, childState);
            if (curLayer->isComposited())
                setCompositingParent(curLayer, childState.m_compositingAncestor);
        }
    }
    
    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                rebuildCompositingLayerTree(curLayer, childState);
                if (curLayer->isComposited())
                    setCompositingParent(curLayer, childState.m_compositingAncestor);
            }
        }
    }
}


// Recurs down the RenderLayer tree until its finds the compositing descendants of compositingAncestor and updates their geometry.
void RenderLayerCompositor::updateCompositingChildrenGeometry(RenderLayer* compositingAncestor, RenderLayer* layer)
{
    if (layer != compositingAncestor) {
        if (RenderLayerBacking* layerBacking = layer->backing()) {
            layerBacking->setCompositedBounds(calculateCompositedBounds(layer, layer));
            layerBacking->updateGraphicsLayerGeometry();
            return;
        }
    }

    if (!layer->hasCompositingDescendant())
        return;
    
    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i)
                updateCompositingChildrenGeometry(compositingAncestor, negZOrderList->at(i));
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i)
            updateCompositingChildrenGeometry(compositingAncestor, normalFlowList->at(i));
    }
    
    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i)
                updateCompositingChildrenGeometry(compositingAncestor, posZOrderList->at(i));
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
        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                int x = 0, y = 0;
                curLayer->convertToLayerCoords(layer, x, y);
                IntRect childRect(rect);
                childRect.move(-x, -y);
                recursiveRepaintLayerRect(curLayer, childRect);
            }
        }

        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                int x = 0, y = 0;
                curLayer->convertToLayerCoords(layer, x, y);
                IntRect childRect(rect);
                childRect.move(-x, -y);
                recursiveRepaintLayerRect(curLayer, childRect);
            }
        }
        
        if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
            size_t listSize = normalFlowList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = normalFlowList->at(i);
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
        m_rootPlatformLayer->setSize(FloatSize(m_renderView->overflowWidth(), m_renderView->overflowHeight()));
}

bool RenderLayerCompositor::has3DContent() const
{
    return layerHas3DContent(rootRenderLayer());
}

bool RenderLayerCompositor::needsToBeComposited(const RenderLayer* layer) const
{
    return m_hasAcceleratedCompositing && (requiresCompositingLayer(layer) || layer->mustOverlayCompositedLayers());
}

// Note: this specifies whether the RL needs a compositing layer for intrinsic reasons.
// Use needsToBeComposited() to determine if a RL actually needs a compositing layer.
// static
bool RenderLayerCompositor::requiresCompositingLayer(const RenderLayer* layer) const
{
    // The root layer always has a compositing layer, but it may not have backing.
    return (inCompositingMode() && layer->isRootLayer()) ||
             requiresCompositingForTransform(layer->renderer()) ||
             requiresCompositingForVideo(layer->renderer()) ||
             layer->renderer()->style()->backfaceVisibility() == BackfaceVisibilityHidden ||
             clipsCompositingDescendants(layer) ||
             requiresCompositingForAnimation(layer->renderer());
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
    if (!compositingAncestor)
        return false;

    // If the compositingAncestor clips, that will be taken care of by clipsCompositingDescendants(),
    // so we only care about clipping between its first child that is our ancestor (the computeClipRoot),
    // and layer.
    RenderLayer* computeClipRoot = 0;
    RenderLayer* curr = layer;
    while (curr) {
        RenderLayer* next = curr->parent();
        if (next == compositingAncestor) {
            computeClipRoot = curr;
            break;
        }
        curr = next;
    }
    
    if (!computeClipRoot || computeClipRoot == layer)
        return false;

    ClipRects parentRects;
    layer->parentClipRects(computeClipRoot, parentRects, true);

    return parentRects.overflowClipRect() != ClipRects::infiniteRect();
}

// Return true if the given layer is a stacking context and has compositing child
// layers that it needs to clip. In this case we insert a clipping GraphicsLayer
// into the hierarchy between this layer and its children in the z-order hierarchy.
bool RenderLayerCompositor::clipsCompositingDescendants(const RenderLayer* layer) const
{
    // FIXME: need to look at hasClip() too eventually
    return layer->hasCompositingDescendant() &&
           layer->renderer()->hasOverflowClip();
}

bool RenderLayerCompositor::requiresCompositingForTransform(RenderObject* renderer) const
{
    RenderStyle* style = renderer->style();
    // Note that we ask the renderer if it has a transform, because the style may have transforms,
    // but the renderer may be an inline that doesn't suppport them.
    return renderer->hasTransform() && (style->transform().has3DOperation() || style->transformStyle3D() == TransformStyle3DPreserve3D || style->hasPerspective());
}

bool RenderLayerCompositor::requiresCompositingForVideo(RenderObject* renderer) const
{
#if ENABLE(VIDEO)
    if (renderer->isVideo()) {
        RenderVideo* video = static_cast<RenderVideo*>(renderer);
        return canAccelerateVideoRendering(video);
    }
#endif
    return false;
}

bool RenderLayerCompositor::requiresCompositingForAnimation(RenderObject* renderer) const
{
    if (AnimationController* animController = renderer->animation()) {
        return (animController->isAnimatingPropertyOnRenderer(renderer, CSSPropertyOpacity) && inCompositingMode())
            || animController->isAnimatingPropertyOnRenderer(renderer, CSSPropertyWebkitTransform);
    }
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
    m_rootPlatformLayer->setSize(FloatSize(m_renderView->overflowWidth(), m_renderView->overflowHeight()));
    m_rootPlatformLayer->setPosition(FloatPoint(0, 0));

    if (GraphicsLayer::compositingCoordinatesOrientation() == GraphicsLayer::CompositingCoordinatesBottomUp)
        m_rootPlatformLayer->setChildrenTransform(flipTransform());

    // Need to clip to prevent transformed content showing outside this frame
    m_rootPlatformLayer->setMasksToBounds(true);
    
    didMoveOnscreen();
}

void RenderLayerCompositor::destroyRootPlatformLayer()
{
    if (!m_rootPlatformLayer)
        return;

    willMoveOffscreen();
    delete m_rootPlatformLayer;
    m_rootPlatformLayer = 0;
}

bool RenderLayerCompositor::layerHas3DContent(const RenderLayer* layer) const
{
    const RenderStyle* style = layer->renderer()->style();

    if (style && 
        (style->transformStyle3D() == TransformStyle3DPreserve3D ||
         style->hasPerspective() ||
         style->transform().has3DOperation()))
        return true;

    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                if (layerHas3DContent(curLayer))
                    return true;
            }
        }

        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                if (layerHas3DContent(curLayer))
                    return true;
            }
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            if (layerHas3DContent(curLayer))
                return true;
        }
    }
    return false;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

