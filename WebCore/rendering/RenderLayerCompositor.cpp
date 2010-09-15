/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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
#include "CanvasRenderingContext.h"
#include "CSSPropertyNames.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "HTMLCanvasElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "NodeList.h"
#include "Page.h"
#include "RenderEmbeddedObject.h"
#include "RenderIFrame.h"
#include "RenderLayerBacking.h"
#include "RenderReplica.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "Settings.h"

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLMediaElement.h"
#endif

#if PROFILE_LAYER_REBUILD
#include <wtf/CurrentTime.h>
#endif

#ifndef NDEBUG
#include "RenderTreeAsText.h"
#endif

#if ENABLE(3D_RENDERING)
// This symbol is used to determine from a script whether 3D rendering is enabled (via 'nm').
bool WebCoreHas3DRendering = true;
#endif

namespace WebCore {

using namespace HTMLNames;

struct CompositingState {
    CompositingState(RenderLayer* compAncestor)
        : m_compositingAncestor(compAncestor)
        , m_subtreeIsCompositing(false)
#ifndef NDEBUG
        , m_depth(0)
#endif
    {
    }
    
    RenderLayer* m_compositingAncestor;
    bool m_subtreeIsCompositing;
#ifndef NDEBUG
    int m_depth;
#endif
};

RenderLayerCompositor::RenderLayerCompositor(RenderView* renderView)
    : m_renderView(renderView)
    , m_rootPlatformLayer(0)
    , m_updateCompositingLayersTimer(this, &RenderLayerCompositor::updateCompositingLayersTimerFired)
    , m_hasAcceleratedCompositing(true)
    , m_showDebugBorders(false)
    , m_showRepaintCounter(false)
    , m_compositingConsultsOverlap(true)
    , m_compositingDependsOnGeometry(false)
    , m_compositing(false)
    , m_compositingLayersNeedRebuild(false)
    , m_rootLayerAttachment(RootLayerUnattached)
#if PROFILE_LAYER_REBUILD
    , m_rootLayerUpdateCount(0)
#endif // PROFILE_LAYER_REBUILD
{
}

RenderLayerCompositor::~RenderLayerCompositor()
{
    ASSERT(m_rootLayerAttachment == RootLayerUnattached);
}

void RenderLayerCompositor::enableCompositingMode(bool enable /* = true */)
{
    if (enable != m_compositing) {
        m_compositing = enable;
        
        if (m_compositing) {
            ensureRootPlatformLayer();
            notifyIFramesOfCompositingChange();
        } else
            destroyRootPlatformLayer();
    }
}

void RenderLayerCompositor::cacheAcceleratedCompositingFlags()
{
    bool hasAcceleratedCompositing = false;
    bool showDebugBorders = false;
    bool showRepaintCounter = false;
    
    if (Settings* settings = m_renderView->document()->settings()) {
        hasAcceleratedCompositing = settings->acceleratedCompositingEnabled();
        showDebugBorders = settings->showDebugBorders();
        showRepaintCounter = settings->showRepaintCounter();
    }

    // We allow the chrome to override the settings, in case the page is rendered
    // on a chrome that doesn't allow accelerated compositing.
    if (hasAcceleratedCompositing) {
        Frame* frame = m_renderView->frameView()->frame();
        Page* page = frame ? frame->page() : 0;
        if (page)
            hasAcceleratedCompositing = page->chrome()->client()->allowsAcceleratedCompositing();
    }

    if (hasAcceleratedCompositing != m_hasAcceleratedCompositing || showDebugBorders != m_showDebugBorders || showRepaintCounter != m_showRepaintCounter)
        setCompositingLayersNeedRebuild();
        
    m_hasAcceleratedCompositing = hasAcceleratedCompositing;
    m_showDebugBorders = showDebugBorders;
    m_showRepaintCounter = showRepaintCounter;
}

void RenderLayerCompositor::setCompositingLayersNeedRebuild(bool needRebuild)
{
    if (inCompositingMode())
        m_compositingLayersNeedRebuild = needRebuild;
}

void RenderLayerCompositor::scheduleSync()
{
    Frame* frame = m_renderView->frameView()->frame();
    Page* page = frame ? frame->page() : 0;
    if (!page)
        return;

    page->chrome()->client()->scheduleCompositingLayerSync();
}

void RenderLayerCompositor::scheduleCompositingLayerUpdate()
{
    if (!m_updateCompositingLayersTimer.isActive())
        m_updateCompositingLayersTimer.startOneShot(0);
}

bool RenderLayerCompositor::compositingLayerUpdatePending() const
{
    return m_updateCompositingLayersTimer.isActive();
}

void RenderLayerCompositor::updateCompositingLayersTimerFired(Timer<RenderLayerCompositor>*)
{
    updateCompositingLayers();
}

void RenderLayerCompositor::updateCompositingLayers(CompositingUpdateType updateType, RenderLayer* updateRoot)
{
    m_updateCompositingLayersTimer.stop();

    if (!m_compositingDependsOnGeometry && !m_compositing)
        return;

    bool checkForHierarchyUpdate = m_compositingDependsOnGeometry;
    bool needGeometryUpdate = false;
    
    switch (updateType) {
    case CompositingUpdateAfterLayoutOrStyleChange:
    case CompositingUpdateOnPaitingOrHitTest:
        checkForHierarchyUpdate = true;
        break;
    case CompositingUpdateOnScroll:
        if (m_compositingConsultsOverlap)
            checkForHierarchyUpdate = true; // Overlap can change with scrolling, so need to check for hierarchy updates.

        needGeometryUpdate = true;
        break;
    }

    if (!checkForHierarchyUpdate && !needGeometryUpdate)
        return;

    bool needHierarchyUpdate = m_compositingLayersNeedRebuild;
    if (!updateRoot || m_compositingConsultsOverlap) {
        // Only clear the flag if we're updating the entire hierarchy.
        m_compositingLayersNeedRebuild = false;
        updateRoot = rootRenderLayer();
    }

#if PROFILE_LAYER_REBUILD
    ++m_rootLayerUpdateCount;
    
    double startTime = WTF::currentTime();
#endif        

    if (checkForHierarchyUpdate) {
        // Go through the layers in presentation order, so that we can compute which RenderLayers need compositing layers.
        // FIXME: we could maybe do this and the hierarchy udpate in one pass, but the parenting logic would be more complex.
        CompositingState compState(updateRoot);
        bool layersChanged = false;
        if (m_compositingConsultsOverlap) {
            OverlapMap overlapTestRequestMap;
            computeCompositingRequirements(updateRoot, &overlapTestRequestMap, compState, layersChanged);
        } else
            computeCompositingRequirements(updateRoot, 0, compState, layersChanged);
        
        needHierarchyUpdate |= layersChanged;
    }

    if (needHierarchyUpdate) {
        // Update the hierarchy of the compositing layers.
        CompositingState compState(updateRoot);
        Vector<GraphicsLayer*> childList;
        rebuildCompositingLayerTree(updateRoot, compState, childList);

        // Host the document layer in the RenderView's root layer.
        if (updateRoot == rootRenderLayer()) {
            if (childList.isEmpty())
                destroyRootPlatformLayer();
            else
                m_rootPlatformLayer->setChildren(childList);
        }
    } else if (needGeometryUpdate) {
        // We just need to do a geometry update. This is only used for position:fixed scrolling;
        // most of the time, geometry is updated via RenderLayer::styleChanged().
        updateLayerTreeGeometry(updateRoot);
    }
    
#if PROFILE_LAYER_REBUILD
    double endTime = WTF::currentTime();
    if (updateRoot == rootRenderLayer())
        fprintf(stderr, "Update %d: computeCompositingRequirements for the world took %fms\n",
                    m_rootLayerUpdateCount, 1000.0 * (endTime - startTime));
#endif
    ASSERT(updateRoot || !m_compositingLayersNeedRebuild);

    if (!hasAcceleratedCompositing())
        enableCompositingMode(false);
}

bool RenderLayerCompositor::updateBacking(RenderLayer* layer, CompositingChangeRepaint shouldRepaint)
{
    bool layerChanged = false;

    if (needsToBeComposited(layer)) {
        enableCompositingMode();
        
        // 3D transforms turn off the testing of overlap.
        if (requiresCompositingForTransform(layer->renderer()))
            setCompositingConsultsOverlap(false);

        if (!layer->backing()) {

            // If we need to repaint, do so before making backing
            if (shouldRepaint == CompositingChangeRepaintNow)
                repaintOnCompositingChange(layer);

            layer->ensureBacking();
            layerChanged = true;
        }
    } else {
        if (layer->backing()) {
            // If we're removing backing on a reflection, clear the source GraphicsLayer's pointer to
            // its replica GraphicsLayer. In practice this should never happen because reflectee and reflection 
            // are both either composited, or not composited.
            if (layer->isReflection()) {
                RenderLayer* sourceLayer = toRenderBoxModelObject(layer->renderer()->parent())->layer();
                if (RenderLayerBacking* backing = sourceLayer->backing()) {
                    ASSERT(backing->graphicsLayer()->replicaLayer() == layer->backing()->graphicsLayer());
                    backing->graphicsLayer()->setReplicatedByLayer(0);
                }
            }
            
            layer->clearBacking();
            layerChanged = true;

            // The layer's cached repaints rects are relative to the repaint container, so change when
            // compositing changes; we need to update them here.
            layer->computeRepaintRects();

            // If we need to repaint, do so now that we've removed the backing
            if (shouldRepaint == CompositingChangeRepaintNow)
                repaintOnCompositingChange(layer);
        }
    }
    
#if ENABLE(VIDEO)
    if (layerChanged && layer->renderer()->isVideo()) {
        // If it's a video, give the media player a chance to hook up to the layer.
        RenderVideo* video = toRenderVideo(layer->renderer());
        video->acceleratedRenderingStateChanged();
    }
#endif

    if (layerChanged && layer->renderer()->isRenderIFrame()) {
        RenderLayerCompositor* innerCompositor = iframeContentsCompositor(toRenderIFrame(layer->renderer()));
        if (innerCompositor && innerCompositor->inCompositingMode())
            innerCompositor->updateRootLayerAttachment();
    }

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
    // If the renderer is not attached yet, no need to repaint.
    if (layer->renderer() != m_renderView && !layer->renderer()->parent())
        return;

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
    if (!canBeComposited(layer))
        return IntRect();

    IntRect boundingBoxRect = layer->localBoundingBox();
    if (layer->renderer()->isRoot()) {
        // If the root layer becomes composited (e.g. because some descendant with negative z-index is composited),
        // then it has to be big enough to cover the viewport in order to display the background. This is akin
        // to the code in RenderBox::paintRootBoxDecorations().
        if (m_renderView->frameView()) {
            int rw = m_renderView->frameView()->contentsWidth();
            int rh = m_renderView->frameView()->contentsHeight();
            
            boundingBoxRect.setWidth(max(boundingBoxRect.width(), rw - boundingBoxRect.x()));
            boundingBoxRect.setHeight(max(boundingBoxRect.height(), rh - boundingBoxRect.y()));
        }
    }
    
    IntRect unionBounds = boundingBoxRect;
    
    if (layer->renderer()->hasOverflowClip() || layer->renderer()->hasMask()) {
        int ancestorRelX = 0, ancestorRelY = 0;
        layer->convertToLayerCoords(ancestorLayer, ancestorRelX, ancestorRelY);
        boundingBoxRect.move(ancestorRelX, ancestorRelY);
        return boundingBoxRect;
    }

    if (RenderLayer* reflection = layer->reflectionLayer()) {
        if (!reflection->isComposited()) {
            IntRect childUnionBounds = calculateCompositedBounds(reflection, layer);
            unionBounds.unite(childUnionBounds);
        }
    }
    
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

    if (layer->paintsWithTransform(PaintBehaviorNormal)) {
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
    setCompositingLayersNeedRebuild();
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

    setCompositingLayersNeedRebuild();
}

RenderLayer* RenderLayerCompositor::enclosingNonStackingClippingLayer(const RenderLayer* layer) const
{
    for (RenderLayer* curr = layer->parent(); curr != 0; curr = curr->parent()) {
        if (curr->isStackingContext())
            return 0;

        if (curr->renderer()->hasOverflowClip() || curr->renderer()->hasClip())
            return curr;
    }
    return 0;
}

void RenderLayerCompositor::addToOverlapMap(OverlapMap& overlapMap, RenderLayer* layer, IntRect& layerBounds, bool& boundsComputed)
{
    if (layer->isRootLayer())
        return;

    if (!boundsComputed) {
        layerBounds = layer->renderer()->localToAbsoluteQuad(FloatRect(layer->localBoundingBox())).enclosingBoundingBox();
        // Empty rects never intersect, but we need them to for the purposes of overlap testing.
        if (layerBounds.isEmpty())
            layerBounds.setSize(IntSize(1, 1));
        boundsComputed = true;
    }

    overlapMap.add(layer, layerBounds);
}

bool RenderLayerCompositor::overlapsCompositedLayers(OverlapMap& overlapMap, const IntRect& layerBounds)
{
    RenderLayerCompositor::OverlapMap::const_iterator end = overlapMap.end();
    for (RenderLayerCompositor::OverlapMap::const_iterator it = overlapMap.begin(); it != end; ++it) {
        const IntRect& bounds = it->second;
        if (layerBounds.intersects(bounds))
            return true;
    }
    
    return false;
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
void RenderLayerCompositor::computeCompositingRequirements(RenderLayer* layer, OverlapMap* overlapMap, struct CompositingState& compositingState, bool& layersChanged)
{
    layer->updateLayerPosition();
    layer->updateZOrderLists();
    layer->updateNormalFlowList();
    
    // Clear the flag
    layer->setHasCompositingDescendant(false);
    
    bool mustOverlapCompositedLayers = compositingState.m_subtreeIsCompositing;

    bool haveComputedBounds = false;
    IntRect absBounds;
    if (overlapMap && !overlapMap->isEmpty()) {
        // If we're testing for overlap, we only need to composite if we overlap something that is already composited.
        absBounds = layer->renderer()->localToAbsoluteQuad(FloatRect(layer->localBoundingBox())).enclosingBoundingBox();
        // Empty rects never intersect, but we need them to for the purposes of overlap testing.
        if (absBounds.isEmpty())
            absBounds.setSize(IntSize(1, 1));
        haveComputedBounds = true;
        mustOverlapCompositedLayers = overlapsCompositedLayers(*overlapMap, absBounds);
    }
    
    layer->setMustOverlapCompositedLayers(mustOverlapCompositedLayers);
    
    // The children of this layer don't need to composite, unless there is
    // a compositing layer among them, so start by inheriting the compositing
    // ancestor with m_subtreeIsCompositing set to false.
    CompositingState childState(compositingState.m_compositingAncestor);
#ifndef NDEBUG
    ++childState.m_depth;
#endif

    bool willBeComposited = needsToBeComposited(layer);
    if (willBeComposited) {
        // Tell the parent it has compositing descendants.
        compositingState.m_subtreeIsCompositing = true;
        // This layer now acts as the ancestor for kids.
        childState.m_compositingAncestor = layer;
        if (overlapMap)
            addToOverlapMap(*overlapMap, layer, absBounds, haveComputedBounds);
    }

#if ENABLE(VIDEO)
    // Video is special. It's a replaced element with a content layer, but has shadow content
    // for the controller that must render in front. Without this, the controls fail to show
    // when the video element is a stacking context (e.g. due to opacity or transform).
    if (willBeComposited && layer->renderer()->isVideo())
        childState.m_subtreeIsCompositing = true;
#endif

    if (layer->isStackingContext()) {
        ASSERT(!layer->m_zOrderListsDirty);
        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                computeCompositingRequirements(curLayer, overlapMap, childState, layersChanged);

                // If we have to make a layer for this child, make one now so we can have a contents layer
                // (since we need to ensure that the -ve z-order child renders underneath our contents).
                if (!willBeComposited && childState.m_subtreeIsCompositing) {
                    // make layer compositing
                    layer->setMustOverlapCompositedLayers(true);
                    childState.m_compositingAncestor = layer;
                    if (overlapMap)
                        addToOverlapMap(*overlapMap, layer, absBounds, haveComputedBounds);
                    willBeComposited = true;
                }
            }
        }
    }
    
    ASSERT(!layer->m_normalFlowListDirty);
    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            computeCompositingRequirements(curLayer, overlapMap, childState, layersChanged);
        }
    }

    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                computeCompositingRequirements(curLayer, overlapMap, childState, layersChanged);
            }
        }
    }
    
    // If we just entered compositing mode, the root will have become composited (as long as accelerated compositing is enabled).
    if (layer->isRootLayer()) {
        if (inCompositingMode() && m_hasAcceleratedCompositing)
            willBeComposited = true;
    }
    
    ASSERT(willBeComposited == needsToBeComposited(layer));

    // If we have a software transform, and we have layers under us, we need to also
    // be composited. Also, if we have opacity < 1, then we need to be a layer so that
    // the child layers are opaque, then rendered with opacity on this layer.
    if (!willBeComposited && canBeComposited(layer) && childState.m_subtreeIsCompositing && requiresCompositingWhenDescendantsAreCompositing(layer->renderer())) {
        layer->setMustOverlapCompositedLayers(true);
        if (overlapMap)
            addToOverlapMap(*overlapMap, layer, absBounds, haveComputedBounds);
        willBeComposited = true;
    }

    ASSERT(willBeComposited == needsToBeComposited(layer));
    if (layer->reflectionLayer())
        layer->reflectionLayer()->setMustOverlapCompositedLayers(willBeComposited);

    // Subsequent layers in the parent stacking context also need to composite.
    if (childState.m_subtreeIsCompositing)
        compositingState.m_subtreeIsCompositing = true;

    // Set the flag to say that this SC has compositing children.
    layer->setHasCompositingDescendant(childState.m_subtreeIsCompositing);

    // setHasCompositingDescendant() may have changed the answer to needsToBeComposited() when clipping,
    // so test that again.
    if (!willBeComposited && canBeComposited(layer) && clipsCompositingDescendants(layer)) {
        if (overlapMap)
            addToOverlapMap(*overlapMap, layer, absBounds, haveComputedBounds);
        willBeComposited = true;
    }

    // If we're back at the root, and no other layers need to be composited, and the root layer itself doesn't need
    // to be composited, then we can drop out of compositing mode altogether.
    if (layer->isRootLayer() && !childState.m_subtreeIsCompositing && !requiresCompositingLayer(layer)) {
        enableCompositingMode(false);
        willBeComposited = false;
    }
    
    // If the layer is going into compositing mode, repaint its old location.
    ASSERT(willBeComposited == needsToBeComposited(layer));
    if (!layer->isComposited() && willBeComposited)
        repaintOnCompositingChange(layer);

    // Update backing now, so that we can use isComposited() reliably during tree traversal in rebuildCompositingLayerTree().
    if (updateBacking(layer, CompositingChangeRepaintNow))
        layersChanged = true;

    if (layer->reflectionLayer() && updateLayerCompositingState(layer->reflectionLayer(), CompositingChangeRepaintNow))
        layersChanged = true;
}

void RenderLayerCompositor::setCompositingParent(RenderLayer* childLayer, RenderLayer* parentLayer)
{
    ASSERT(!parentLayer || childLayer->ancestorCompositingLayer() == parentLayer);
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

#if ENABLE(VIDEO)
bool RenderLayerCompositor::canAccelerateVideoRendering(RenderVideo* o) const
{
    if (!m_hasAcceleratedCompositing)
        return false;

    return o->supportsAcceleratedRendering();
}
#endif

void RenderLayerCompositor::rebuildCompositingLayerTree(RenderLayer* layer, const CompositingState& compositingState, Vector<GraphicsLayer*>& childLayersOfEnclosingLayer)
{
    // Make the layer compositing if necessary, and set up clipping and content layers.
    // Note that we can only do work here that is independent of whether the descendant layers
    // have been processed. computeCompositingRequirements() will already have done the repaint if necessary.
    
    RenderLayerBacking* layerBacking = layer->backing();
    if (layerBacking) {
        // The compositing state of all our children has been updated already, so now
        // we can compute and cache the composited bounds for this layer.
        layerBacking->updateCompositedBounds();

        if (RenderLayer* reflection = layer->reflectionLayer()) {
            if (reflection->backing())
                reflection->backing()->updateCompositedBounds();
        }

        layerBacking->updateGraphicsLayerConfiguration();
        layerBacking->updateGraphicsLayerGeometry();

        if (!layer->parent())
            updateRootLayerPosition();
    }

    // If this layer has backing, then we are collecting its children, otherwise appending
    // to the compositing child list of an enclosing layer.
    Vector<GraphicsLayer*> layerChildren;
    Vector<GraphicsLayer*>& childList = layerBacking ? layerChildren : childLayersOfEnclosingLayer;

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
                rebuildCompositingLayerTree(curLayer, childState, childList);
            }
        }

        // If a negative z-order child is compositing, we get a foreground layer which needs to get parented.
        if (layerBacking && layerBacking->foregroundLayer())
            childList.append(layerBacking->foregroundLayer());
    }

    ASSERT(!layer->m_normalFlowListDirty);
    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            rebuildCompositingLayerTree(curLayer, childState, childList);
        }
    }
    
    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                rebuildCompositingLayerTree(curLayer, childState, childList);
            }
        }
    }
    
    if (layerBacking) {
        bool parented = false;
        if (layer->renderer()->isRenderIFrame())
            parented = parentIFrameContentLayers(toRenderIFrame(layer->renderer()));
        
        if (!parented)
            layerBacking->parentForSublayers()->setChildren(layerChildren);

        childLayersOfEnclosingLayer.append(layerBacking->childForSuperlayers());
    }
}

void RenderLayerCompositor::frameViewDidChangeSize(const IntPoint& contentsOffset)
{
    if (m_clipLayer) {
        FrameView* frameView = m_renderView->frameView();
        m_clipLayer->setPosition(contentsOffset);
        m_clipLayer->setSize(FloatSize(frameView->layoutWidth(), frameView->layoutHeight()));

        IntPoint scrollPosition = frameView->scrollPosition();
        m_scrollLayer->setPosition(FloatPoint(-scrollPosition.x(), -scrollPosition.y()));
    }
}

void RenderLayerCompositor::frameViewDidScroll(const IntPoint& scrollPosition)
{
    if (m_scrollLayer)
        m_scrollLayer->setPosition(FloatPoint(-scrollPosition.x(), -scrollPosition.y()));
}

RenderLayerCompositor* RenderLayerCompositor::iframeContentsCompositor(RenderIFrame* renderer)
{
    HTMLIFrameElement* element = static_cast<HTMLIFrameElement*>(renderer->node());
    if (Document* contentDocument = element->contentDocument()) {
        if (RenderView* view = contentDocument->renderView())
            return view->compositor();
    }
    return 0;
}

bool RenderLayerCompositor::parentIFrameContentLayers(RenderIFrame* renderer)
{
    RenderLayerCompositor* innerCompositor = iframeContentsCompositor(renderer);
    if (!innerCompositor || !innerCompositor->inCompositingMode() || innerCompositor->rootLayerAttachment() != RootLayerAttachedViaEnclosingIframe)
        return false;
    
    RenderLayer* layer = renderer->layer();
    if (!layer->isComposited())
        return false;

    RenderLayerBacking* backing = layer->backing();
    GraphicsLayer* hostingLayer = backing->parentForSublayers();
    GraphicsLayer* rootLayer = innerCompositor->rootPlatformLayer();
    if (hostingLayer->children().size() != 1 || hostingLayer->children()[0] != rootLayer) {
        hostingLayer->removeAllChildren();
        hostingLayer->addChild(rootLayer);
    }
    return true;
}

// This just updates layer geometry without changing the hierarchy.
void RenderLayerCompositor::updateLayerTreeGeometry(RenderLayer* layer)
{
    if (RenderLayerBacking* layerBacking = layer->backing()) {
        // The compositing state of all our children has been updated already, so now
        // we can compute and cache the composited bounds for this layer.
        layerBacking->updateCompositedBounds();

        if (RenderLayer* reflection = layer->reflectionLayer()) {
            if (reflection->backing())
                reflection->backing()->updateCompositedBounds();
        }

        layerBacking->updateGraphicsLayerConfiguration();
        layerBacking->updateGraphicsLayerGeometry();

        if (!layer->parent())
            updateRootLayerPosition();
    }

    if (layer->isStackingContext()) {
        ASSERT(!layer->m_zOrderListsDirty);

        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i)
                updateLayerTreeGeometry(negZOrderList->at(i));
        }
    }

    ASSERT(!layer->m_normalFlowListDirty);
    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i)
            updateLayerTreeGeometry(normalFlowList->at(i));
    }
    
    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i)
                updateLayerTreeGeometry(posZOrderList->at(i));
        }
    }
}

// Recurs down the RenderLayer tree until its finds the compositing descendants of compositingAncestor and updates their geometry.
void RenderLayerCompositor::updateCompositingDescendantGeometry(RenderLayer* compositingAncestor, RenderLayer* layer, RenderLayerBacking::UpdateDepth updateDepth)
{
    if (layer != compositingAncestor) {
        if (RenderLayerBacking* layerBacking = layer->backing()) {
            layerBacking->updateCompositedBounds();

            if (RenderLayer* reflection = layer->reflectionLayer()) {
                if (reflection->backing())
                    reflection->backing()->updateCompositedBounds();
            }

            layerBacking->updateGraphicsLayerGeometry();
            if (updateDepth == RenderLayerBacking::CompositingChildren)
                return;
        }
    }

    if (layer->reflectionLayer())
        updateCompositingDescendantGeometry(compositingAncestor, layer->reflectionLayer(), updateDepth);

    if (!layer->hasCompositingDescendant())
        return;
    
    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i)
                updateCompositingDescendantGeometry(compositingAncestor, negZOrderList->at(i), updateDepth);
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i)
            updateCompositingDescendantGeometry(compositingAncestor, normalFlowList->at(i), updateDepth);
    }
    
    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i)
                updateCompositingDescendantGeometry(compositingAncestor, posZOrderList->at(i), updateDepth);
        }
    }
}


void RenderLayerCompositor::repaintCompositedLayersAbsoluteRect(const IntRect& absRect)
{
    recursiveRepaintLayerRect(rootRenderLayer(), absRect);
}

void RenderLayerCompositor::recursiveRepaintLayerRect(RenderLayer* layer, const IntRect& rect)
{
    // FIXME: This method does not work correctly with transforms.
    if (layer->isComposited())
        layer->setBackingNeedsRepaintInRect(rect);

    if (layer->hasCompositingDescendant()) {
        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                int x = 0;
                int y = 0;
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
                int x = 0;
                int y = 0;
                curLayer->convertToLayerCoords(layer, x, y);
                IntRect childRect(rect);
                childRect.move(-x, -y);
                recursiveRepaintLayerRect(curLayer, childRect);
            }
        }
    }
    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            int x = 0;
            int y = 0;
            curLayer->convertToLayerCoords(layer, x, y);
            IntRect childRect(rect);
            childRect.move(-x, -y);
            recursiveRepaintLayerRect(curLayer, childRect);
        }
    }
}

RenderLayer* RenderLayerCompositor::rootRenderLayer() const
{
    return m_renderView->layer();
}

GraphicsLayer* RenderLayerCompositor::rootPlatformLayer() const
{
    return m_clipLayer ? m_clipLayer.get() : m_rootPlatformLayer.get();
}

void RenderLayerCompositor::didMoveOnscreen()
{
    if (!inCompositingMode() || m_rootLayerAttachment != RootLayerUnattached)
        return;

    RootLayerAttachment attachment = shouldPropagateCompositingToEnclosingIFrame() ? RootLayerAttachedViaEnclosingIframe : RootLayerAttachedViaChromeClient;
    attachRootPlatformLayer(attachment);
}

void RenderLayerCompositor::willMoveOffscreen()
{
    if (!inCompositingMode() || m_rootLayerAttachment == RootLayerUnattached)
        return;

    detachRootPlatformLayer();
}

void RenderLayerCompositor::updateRootLayerPosition()
{
    // Eventually we will need to account for scrolling here.
    // https://bugs.webkit.org/show_bug.cgi?id=38518
    if (m_rootPlatformLayer)
        m_rootPlatformLayer->setSize(FloatSize(m_renderView->rightLayoutOverflow(), m_renderView->bottomLayoutOverflow()));
}

void RenderLayerCompositor::didStartAcceleratedAnimation()
{
    // If an accelerated animation or transition runs, we have to turn off overlap checking because
    // we don't do layout for every frame, but we have to ensure that the layering is
    // correct between the animating object and other objects on the page.
    setCompositingConsultsOverlap(false);
}

bool RenderLayerCompositor::has3DContent() const
{
    return layerHas3DContent(rootRenderLayer());
}

bool RenderLayerCompositor::shouldPropagateCompositingToEnclosingIFrame() const
{
    // Parent document content needs to be able to render on top of a composited iframe, so correct behavior
    // is to have the parent document become composited too. However, this can cause problems on platforms that
    // use native views for frames (like Mac), so disable that behavior on those platforms for now.
    HTMLFrameOwnerElement* ownerElement = enclosingIFrameElement();
    RenderObject* renderer = ownerElement ? ownerElement->renderer() : 0;
    if (!renderer || !renderer->isRenderIFrame())
        return false;

#if !PLATFORM(MAC)
    // On non-Mac platforms, let compositing propagate for all iframes.
    return true;
#else
    // If we're viewless (i.e. WebKit2), we always propagate compositing.
    if (!m_renderView->frameView()->platformWidget())
        return true;

    // On Mac, only propagate compositing if the iframe is overlapped in the parent
    // document, or the parent is already compositing.
    RenderIFrame* iframeRenderer = toRenderIFrame(renderer);
    if (iframeRenderer->widget()) {
        ASSERT(iframeRenderer->widget()->isFrameView());
        FrameView* view = static_cast<FrameView*>(iframeRenderer->widget());
        if (view->isOverlapped())
            return true;
        
        if (RenderView* view = iframeRenderer->view()) {
            if (view->compositor()->inCompositingMode())
                return true;
        }
    }

    return false;
#endif
}

HTMLFrameOwnerElement* RenderLayerCompositor::enclosingIFrameElement() const
{
    if (HTMLFrameOwnerElement* ownerElement = m_renderView->document()->ownerElement())
        return ownerElement->hasTagName(iframeTag) ? ownerElement : 0;

    return 0;
}

bool RenderLayerCompositor::needsToBeComposited(const RenderLayer* layer) const
{
    if (!canBeComposited(layer))
        return false;

    return requiresCompositingLayer(layer) || layer->mustOverlapCompositedLayers() || (inCompositingMode() && layer->isRootLayer());
}

// Note: this specifies whether the RL needs a compositing layer for intrinsic reasons.
// Use needsToBeComposited() to determine if a RL actually needs a compositing layer.
// static
bool RenderLayerCompositor::requiresCompositingLayer(const RenderLayer* layer) const
{
    RenderObject* renderer = layer->renderer();
    // The compositing state of a reflection should match that of its reflected layer.
    if (layer->isReflection()) {
        renderer = renderer->parent(); // The RenderReplica's parent is the object being reflected.
        layer = toRenderBoxModelObject(renderer)->layer();
    }
    // The root layer always has a compositing layer, but it may not have backing.
    return requiresCompositingForTransform(renderer)
             || requiresCompositingForVideo(renderer)
             || requiresCompositingForCanvas(renderer)
             || requiresCompositingForPlugin(renderer)
             || requiresCompositingForIFrame(renderer)
             || renderer->style()->backfaceVisibility() == BackfaceVisibilityHidden
             || clipsCompositingDescendants(layer)
             || requiresCompositingForAnimation(renderer);
}

bool RenderLayerCompositor::canBeComposited(const RenderLayer* layer) const
{
    return m_hasAcceleratedCompositing && layer->isSelfPaintingLayer();
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

    IntRect backgroundRect = layer->backgroundClipRect(computeClipRoot, true);
    return backgroundRect != PaintInfo::infiniteRect();
}

// Return true if the given layer is a stacking context and has compositing child
// layers that it needs to clip. In this case we insert a clipping GraphicsLayer
// into the hierarchy between this layer and its children in the z-order hierarchy.
bool RenderLayerCompositor::clipsCompositingDescendants(const RenderLayer* layer) const
{
    return layer->hasCompositingDescendant() &&
           (layer->renderer()->hasOverflowClip() || layer->renderer()->hasClip());
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
        RenderVideo* video = toRenderVideo(renderer);
        return video->shouldDisplayVideo() && canAccelerateVideoRendering(video);
    }
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    else if (renderer->isRenderPart()) {
        if (!m_hasAcceleratedCompositing)
            return false;

        Node* node = renderer->node();
        if (!node || (!node->hasTagName(HTMLNames::videoTag) && !node->hasTagName(HTMLNames::audioTag)))
            return false;

        HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(node);
        return mediaElement->player() ? mediaElement->player()->supportsAcceleratedRendering() : false;
    }
#endif // ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#else
    UNUSED_PARAM(renderer);
#endif
    return false;
}

bool RenderLayerCompositor::requiresCompositingForCanvas(RenderObject* renderer) const
{
    if (renderer->isCanvas()) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(renderer->node());
        return canvas->renderingContext() && canvas->renderingContext()->isAccelerated();
    }
    return false;
}

bool RenderLayerCompositor::requiresCompositingForPlugin(RenderObject* renderer) const
{
    if (!renderer->isEmbeddedObject())
        return false;
    
    RenderEmbeddedObject* embedRenderer = toRenderEmbeddedObject(renderer);
    if (!embedRenderer->allowsAcceleratedCompositing())
        return false;

    m_compositingDependsOnGeometry = true;

    // If we can't reliably know the size of the plugin yet, don't change compositing state.
    if (renderer->needsLayout())
        return embedRenderer->hasLayer() && embedRenderer->layer()->isComposited();

    // Don't go into compositing mode if height or width are zero, or size is 1x1.
    IntRect contentBox = embedRenderer->contentBoxRect();
    return contentBox.height() * contentBox.width() > 1;
}

bool RenderLayerCompositor::requiresCompositingForIFrame(RenderObject* renderer) const
{
    if (!renderer->isRenderIFrame())
        return false;
    
    RenderIFrame* iframeRenderer = toRenderIFrame(renderer);

    if (!iframeRenderer->requiresAcceleratedCompositing())
        return false;

    m_compositingDependsOnGeometry = true;

    RenderLayerCompositor* innerCompositor = iframeContentsCompositor(iframeRenderer);
    if (!innerCompositor->shouldPropagateCompositingToEnclosingIFrame())
        return false;

    // If we can't reliably know the size of the iframe yet, don't change compositing state.
    if (renderer->needsLayout())
        return iframeRenderer->hasLayer() && iframeRenderer->layer()->isComposited();
    
    // Don't go into compositing mode if height or width are zero.
    IntRect contentBox = iframeRenderer->contentBoxRect();
    return contentBox.height() * contentBox.width() > 0;
}

bool RenderLayerCompositor::requiresCompositingForAnimation(RenderObject* renderer) const
{
    if (AnimationController* animController = renderer->animation()) {
        return (animController->isRunningAnimationOnRenderer(renderer, CSSPropertyOpacity) && inCompositingMode())
            || animController->isRunningAnimationOnRenderer(renderer, CSSPropertyWebkitTransform);
    }
    return false;
}

bool RenderLayerCompositor::requiresCompositingWhenDescendantsAreCompositing(RenderObject* renderer) const
{
    return renderer->hasTransform() || renderer->isTransparent() || renderer->hasMask() || renderer->hasReflection();
}

// If an element has negative z-index children, those children render in front of the 
// layer background, so we need an extra 'contents' layer for the foreground of the layer
// object.
bool RenderLayerCompositor::needsContentsCompositingLayer(const RenderLayer* layer) const
{
    return (layer->m_negZOrderList && layer->m_negZOrderList->size() > 0);
}

bool RenderLayerCompositor::requiresScrollLayer(RootLayerAttachment attachment) const
{
    if (attachment == RootLayerAttachedViaEnclosingIframe)
        return true;

#if PLATFORM(MAC)
    // If we're viewless (i.e. WebKit2), we need to scroll ourselves.
    // FIXME: eventually we should do this on other platforms too.
    if (!m_renderView->frameView()->platformWidget())
        return true;
#endif
    return false;
}

void RenderLayerCompositor::ensureRootPlatformLayer()
{
    RootLayerAttachment expectedAttachment = shouldPropagateCompositingToEnclosingIFrame() ? RootLayerAttachedViaEnclosingIframe : RootLayerAttachedViaChromeClient;
    if (expectedAttachment == m_rootLayerAttachment)
         return;

    if (!m_rootPlatformLayer) {
        m_rootPlatformLayer = GraphicsLayer::create(0);
#ifndef NDEBUG
        m_rootPlatformLayer->setName("Root platform");
#endif
        m_rootPlatformLayer->setSize(FloatSize(m_renderView->rightLayoutOverflow(), m_renderView->bottomLayoutOverflow()));
        m_rootPlatformLayer->setPosition(FloatPoint());

        // Need to clip to prevent transformed content showing outside this frame
        m_rootPlatformLayer->setMasksToBounds(true);
    }

    if (requiresScrollLayer(expectedAttachment)) {
        if (!m_clipLayer) {
            ASSERT(!m_scrollLayer);
            // Create a clipping layer if this is an iframe
            m_clipLayer = GraphicsLayer::create(0);
#ifndef NDEBUG
            m_clipLayer->setName("iframe Clipping");
#endif
            m_clipLayer->setMasksToBounds(true);
            
            m_scrollLayer = GraphicsLayer::create(0);
#ifndef NDEBUG
            m_scrollLayer->setName("iframe scrolling");
#endif
            // Hook them up
            m_clipLayer->addChild(m_scrollLayer.get());
            m_scrollLayer->addChild(m_rootPlatformLayer.get());
            
            frameViewDidChangeSize();
            frameViewDidScroll(m_renderView->frameView()->scrollPosition());
        }
    } else {
        if (m_clipLayer) {
            m_clipLayer->removeAllChildren();
            m_clipLayer->removeFromParent();
            m_clipLayer = 0;
            
            m_scrollLayer->removeAllChildren();
            m_scrollLayer = 0;
        }
    }

    // Check to see if we have to change the attachment
    if (m_rootLayerAttachment != RootLayerUnattached)
        detachRootPlatformLayer();

    attachRootPlatformLayer(expectedAttachment);
}

void RenderLayerCompositor::destroyRootPlatformLayer()
{
    if (!m_rootPlatformLayer)
        return;

    detachRootPlatformLayer();
    if (m_clipLayer) {
        m_clipLayer->removeAllChildren();
        m_clipLayer = 0;
        
        m_scrollLayer->removeAllChildren();
        m_scrollLayer = 0;
    }
    ASSERT(!m_scrollLayer);
    m_rootPlatformLayer = 0;
}

void RenderLayerCompositor::attachRootPlatformLayer(RootLayerAttachment attachment)
{
    if (!m_rootPlatformLayer)
        return;

    switch (attachment) {
        case RootLayerUnattached:
            ASSERT_NOT_REACHED();
            break;
        case RootLayerAttachedViaChromeClient: {
            Frame* frame = m_renderView->frameView()->frame();
            Page* page = frame ? frame->page() : 0;
            if (!page)
                return;

            page->chrome()->client()->attachRootGraphicsLayer(frame, rootPlatformLayer());
            break;
        }
        case RootLayerAttachedViaEnclosingIframe: {
            // The layer will get hooked up via RenderLayerBacking::updateGraphicsLayerConfiguration()
            // for the iframe's renderer in the parent document.
            scheduleNeedsStyleRecalc(m_renderView->document()->ownerElement());
            break;
        }
    }
    
    m_rootLayerAttachment = attachment;
    rootLayerAttachmentChanged();
}

void RenderLayerCompositor::detachRootPlatformLayer()
{
    if (!m_rootPlatformLayer || m_rootLayerAttachment == RootLayerUnattached)
        return;

    switch (m_rootLayerAttachment) {
        case RootLayerAttachedViaEnclosingIframe: {
            // The layer will get unhooked up via RenderLayerBacking::updateGraphicsLayerConfiguration()
            // for the iframe's renderer in the parent document.
            if (m_clipLayer)
                m_clipLayer->removeFromParent();
            else
                m_rootPlatformLayer->removeFromParent();

            if (HTMLFrameOwnerElement* ownerElement = m_renderView->document()->ownerElement())
                scheduleNeedsStyleRecalc(ownerElement);
            break;
        }
        case RootLayerAttachedViaChromeClient: {
            Frame* frame = m_renderView->frameView()->frame();
            Page* page = frame ? frame->page() : 0;
            if (!page)
                return;

            page->chrome()->client()->attachRootGraphicsLayer(frame, 0);
        }
        break;
        case RootLayerUnattached:
            break;
    }

    m_rootLayerAttachment = RootLayerUnattached;
    rootLayerAttachmentChanged();
}

void RenderLayerCompositor::updateRootLayerAttachment()
{
    ensureRootPlatformLayer();
}

void RenderLayerCompositor::rootLayerAttachmentChanged()
{
    // The attachment can affect whether the RenderView layer's paintingGoesToWindow() behavior,
    // so call updateGraphicsLayerGeometry() to udpate that.
    RenderLayer* layer = m_renderView->layer();
    if (RenderLayerBacking* backing = layer ? layer->backing() : 0)
        backing->updateDrawsContent();
}

static void needsStyleRecalcCallback(Node* node)
{
    node->setNeedsStyleRecalc(SyntheticStyleChange);
}

void RenderLayerCompositor::scheduleNeedsStyleRecalc(Element* element)
{
    if (ContainerNode::postAttachCallbacksAreSuspended())
        ContainerNode::queuePostAttachCallback(needsStyleRecalcCallback, element);
    else
        element->setNeedsStyleRecalc(SyntheticStyleChange);
}

// IFrames are special, because we hook compositing layers together across iframe boundaries
// when both parent and iframe content are composited. So when this frame becomes composited, we have
// to use a synthetic style change to get the iframes into RenderLayers in order to allow them to composite.
void RenderLayerCompositor::notifyIFramesOfCompositingChange()
{
    Frame* frame = m_renderView->frameView() ? m_renderView->frameView()->frame() : 0;
    if (!frame)
        return;

    for (Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        if (child->document() && child->document()->ownerElement())
            scheduleNeedsStyleRecalc(child->document()->ownerElement());
    }
    
    // Compositing also affects the answer to RenderIFrame::requiresAcceleratedCompositing(), so 
    // we need to schedule a style recalc in our parent document.
    if (HTMLFrameOwnerElement* ownerElement = m_renderView->document()->ownerElement())
        scheduleNeedsStyleRecalc(ownerElement);
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

