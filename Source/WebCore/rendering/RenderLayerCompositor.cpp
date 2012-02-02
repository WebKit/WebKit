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
#include "RenderApplet.h"
#include "RenderEmbeddedObject.h"
#include "RenderFullScreen.h"
#include "RenderIFrame.h"
#include "RenderLayerBacking.h"
#include "RenderReplica.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "ScrollbarTheme.h"
#include "Settings.h"

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLMediaElement.h"
#endif

#if ENABLE(THREADED_SCROLLING)
#include "ScrollingCoordinator.h"
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
    , m_updateCompositingLayersTimer(this, &RenderLayerCompositor::updateCompositingLayersTimerFired)
    , m_hasAcceleratedCompositing(true)
    , m_compositingTriggers(static_cast<ChromeClient::CompositingTriggerFlags>(ChromeClient::AllTriggers))
    , m_compositedLayerCount(0)
    , m_showDebugBorders(false)
    , m_showRepaintCounter(false)
    , m_compositingConsultsOverlap(true)
    , m_compositingDependsOnGeometry(false)
    , m_compositingNeedsUpdate(false)
    , m_compositing(false)
    , m_compositingLayersNeedRebuild(false)
    , m_flushingLayers(false)
    , m_forceCompositingMode(false)
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
            ensureRootLayer();
            notifyIFramesOfCompositingChange();
        } else
            destroyRootLayer();
    }
}

void RenderLayerCompositor::cacheAcceleratedCompositingFlags()
{
    bool hasAcceleratedCompositing = false;
    bool showDebugBorders = false;
    bool showRepaintCounter = false;
    bool forceCompositingMode = false;

    if (Settings* settings = m_renderView->document()->settings()) {
        hasAcceleratedCompositing = settings->acceleratedCompositingEnabled();

        // We allow the chrome to override the settings, in case the page is rendered
        // on a chrome that doesn't allow accelerated compositing.
        if (hasAcceleratedCompositing) {
            Frame* frame = m_renderView->frameView()->frame();
            Page* page = frame ? frame->page() : 0;
            if (page) {
                ChromeClient* chromeClient = page->chrome()->client();
                m_compositingTriggers = chromeClient->allowedCompositingTriggers();
                hasAcceleratedCompositing = m_compositingTriggers;
            }
        }

        showDebugBorders = settings->showDebugBorders();
        showRepaintCounter = settings->showRepaintCounter();
        forceCompositingMode = settings->forceCompositingMode() && hasAcceleratedCompositing;

        if (forceCompositingMode && m_renderView->document()->ownerElement())
            forceCompositingMode = settings->acceleratedCompositingForScrollableFramesEnabled() && requiresCompositingForScrollableFrame();
    }


    if (hasAcceleratedCompositing != m_hasAcceleratedCompositing || showDebugBorders != m_showDebugBorders || showRepaintCounter != m_showRepaintCounter || forceCompositingMode != m_forceCompositingMode)
        setCompositingLayersNeedRebuild();

    m_hasAcceleratedCompositing = hasAcceleratedCompositing;
    m_showDebugBorders = showDebugBorders;
    m_showRepaintCounter = showRepaintCounter;
    m_forceCompositingMode = forceCompositingMode;
}

bool RenderLayerCompositor::canRender3DTransforms() const
{
    return hasAcceleratedCompositing() && (m_compositingTriggers & ChromeClient::ThreeDTransformTrigger);
}

void RenderLayerCompositor::setCompositingLayersNeedRebuild(bool needRebuild)
{
    if (inCompositingMode())
        m_compositingLayersNeedRebuild = needRebuild;
}

void RenderLayerCompositor::scheduleLayerFlush()
{
    Frame* frame = m_renderView->frameView()->frame();
    Page* page = frame ? frame->page() : 0;
    if (!page)
        return;

    page->chrome()->client()->scheduleCompositingLayerSync();
}

void RenderLayerCompositor::flushPendingLayerChanges(bool isFlushRoot)
{
    // FrameView::syncCompositingStateIncludingSubframes() flushes each subframe,
    // but GraphicsLayer::syncCompositingState() will cross frame boundaries
    // if the GraphicsLayers are connected (the RootLayerAttachedViaEnclosingFrame case).
    // As long as we're not the root of the flush, we can bail.
    if (!isFlushRoot && rootLayerAttachment() == RootLayerAttachedViaEnclosingFrame)
        return;
    
    ASSERT(!m_flushingLayers);
    m_flushingLayers = true;

    if (GraphicsLayer* rootLayer = rootGraphicsLayer()) {
        FrameView* frameView = m_renderView ? m_renderView->frameView() : 0;
        if (frameView) {
            // FIXME: Passing frameRect() is correct only when RenderLayerCompositor uses a ScrollLayer (as in WebKit2)
            // otherwise, the passed clip rect needs to take scrolling into account
            rootLayer->syncCompositingState(frameView->frameRect());
        }
    }
    
    ASSERT(m_flushingLayers);
    m_flushingLayers = false;
}

void RenderLayerCompositor::didFlushChangesForLayer(RenderLayer*)
{
}

RenderLayerCompositor* RenderLayerCompositor::enclosingCompositorFlushingLayers() const
{
    if (!m_renderView->frameView())
        return 0;

    for (Frame* frame = m_renderView->frameView()->frame(); frame; frame = frame->tree()->parent()) {
        RenderLayerCompositor* compositor = frame->contentRenderer() ? frame->contentRenderer()->compositor() : 0;
        if (compositor->isFlushingLayers())
            return compositor;
    }
    
    return 0;
}

void RenderLayerCompositor::scheduleCompositingLayerUpdate()
{
    if (!m_updateCompositingLayersTimer.isActive())
        m_updateCompositingLayersTimer.startOneShot(0);
}

void RenderLayerCompositor::updateCompositingLayersTimerFired(Timer<RenderLayerCompositor>*)
{
    updateCompositingLayers();
}

bool RenderLayerCompositor::hasAnyAdditionalCompositedLayers(const RenderLayer* rootLayer) const
{
    return m_compositedLayerCount > (rootLayer->isComposited() ? 1 : 0);
}

void RenderLayerCompositor::updateCompositingLayers(CompositingUpdateType updateType, RenderLayer* updateRoot)
{
    m_updateCompositingLayersTimer.stop();
    
    // Compositing layers will be updated in Document::implicitClose() if suppressed here.
    if (!m_renderView->document()->visualUpdatesAllowed())
        return;

    if (m_forceCompositingMode && !m_compositing)
        enableCompositingMode(true);

    if (!m_compositingDependsOnGeometry && !m_compositing && !m_compositingNeedsUpdate)
        return;

    bool checkForHierarchyUpdate = m_compositingDependsOnGeometry;
    bool needGeometryUpdate = false;

    switch (updateType) {
    case CompositingUpdateAfterLayoutOrStyleChange:
    case CompositingUpdateOnHitTest:
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
        Vector<GraphicsLayer*> childList;
        rebuildCompositingLayerTree(updateRoot, childList);

        // Host the document layer in the RenderView's root layer.
        if (updateRoot == rootRenderLayer()) {
            // Even when childList is empty, don't drop out of compositing mode if there are
            // composited layers that we didn't hit in our traversal (e.g. because of visibility:hidden).
            if (childList.isEmpty() && !hasAnyAdditionalCompositedLayers(updateRoot))
                destroyRootLayer();
            else
                m_rootContentLayer->setChildren(childList);
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

    m_compositingNeedsUpdate = false;
}

bool RenderLayerCompositor::updateBacking(RenderLayer* layer, CompositingChangeRepaint shouldRepaint)
{
    bool layerChanged = false;

    if (needsToBeComposited(layer)) {
        enableCompositingMode();
        
        // Non-identity 3D transforms turn off the testing of overlap.
        if (hasNonIdentity3DTransform(layer->renderer()))
            setCompositingConsultsOverlap(false);

        if (!layer->backing()) {

            // If we need to repaint, do so before making backing
            if (shouldRepaint == CompositingChangeRepaintNow)
                repaintOnCompositingChange(layer);

            layer->ensureBacking();

#if PLATFORM(MAC) && USE(CA)
            Settings* settings = m_renderView->document()->settings();
            if (settings && settings->acceleratedDrawingEnabled())
                layer->backing()->graphicsLayer()->setAcceleratesDrawing(true);
            else if (layer->renderer()->isCanvas()) {
                HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(layer->renderer()->node());
                if (canvas->shouldAccelerate(canvas->size()))
                    layer->backing()->graphicsLayer()->setAcceleratesDrawing(true);
            }
#endif
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

    if (layerChanged && layer->renderer()->isRenderPart()) {
        RenderLayerCompositor* innerCompositor = frameContentsCompositor(toRenderPart(layer->renderer()));
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
LayoutRect RenderLayerCompositor::calculateCompositedBounds(const RenderLayer* layer, const RenderLayer* ancestorLayer)
{
    if (!canBeComposited(layer))
        return LayoutRect();

    LayoutRect boundingBoxRect = layer->localBoundingBox();
    if (layer->renderer()->isRoot()) {
        // If the root layer becomes composited (e.g. because some descendant with negative z-index is composited),
        // then it has to be big enough to cover the viewport in order to display the background. This is akin
        // to the code in RenderBox::paintRootBoxFillLayers().
        if (m_renderView->frameView()) {
            int rw = m_renderView->frameView()->contentsWidth();
            int rh = m_renderView->frameView()->contentsHeight();
            
            boundingBoxRect.setWidth(max(boundingBoxRect.width(), rw - boundingBoxRect.x()));
            boundingBoxRect.setHeight(max(boundingBoxRect.height(), rh - boundingBoxRect.y()));
        }
    }
    
    LayoutRect unionBounds = boundingBoxRect;
    
    if (layer->renderer()->hasOverflowClip() || layer->renderer()->hasMask()) {
        LayoutPoint ancestorRelOffset;
        layer->convertToLayerCoords(ancestorLayer, ancestorRelOffset);
        boundingBoxRect.moveBy(ancestorRelOffset);
        return boundingBoxRect;
    }

    if (RenderLayer* reflection = layer->reflectionLayer()) {
        if (!reflection->isComposited()) {
            LayoutRect childUnionBounds = calculateCompositedBounds(reflection, layer);
            unionBounds.unite(childUnionBounds);
        }
    }
    
    ASSERT(layer->isStackingContext() || (!layer->m_posZOrderList || layer->m_posZOrderList->size() == 0));

    if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
        size_t listSize = negZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = negZOrderList->at(i);
            if (!curLayer->isComposited()) {
                LayoutRect childUnionBounds = calculateCompositedBounds(curLayer, layer);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
        size_t listSize = posZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = posZOrderList->at(i);
            if (!curLayer->isComposited()) {
                LayoutRect childUnionBounds = calculateCompositedBounds(curLayer, layer);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            if (!curLayer->isComposited()) {
                LayoutRect curAbsBounds = calculateCompositedBounds(curLayer, layer);
                unionBounds.unite(curAbsBounds);
            }
        }
    }

    if (layer->paintsWithTransform(PaintBehaviorNormal)) {
        TransformationMatrix* affineTrans = layer->transform();
        boundingBoxRect = affineTrans->mapRect(boundingBoxRect);
        unionBounds = affineTrans->mapRect(unionBounds);
    }

    LayoutPoint ancestorRelOffset;
    layer->convertToLayerCoords(ancestorLayer, ancestorRelOffset);
    unionBounds.moveBy(ancestorRelOffset);

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
        LayoutRect compBounds = child->backing()->compositedBounds();

        LayoutPoint offset;
        child->convertToLayerCoords(compLayer, offset);
        compBounds.moveBy(offset);

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

void RenderLayerCompositor::addToOverlapMap(OverlapMap& overlapMap, RenderLayer* layer, LayoutRect& layerBounds, bool& boundsComputed)
{
    if (layer->isRootLayer())
        return;

    if (!boundsComputed) {
        layerBounds = layer->renderer()->localToAbsoluteQuad(FloatRect(layer->localBoundingBox())).enclosingBoundingBox();
        // Empty rects never intersect, but we need them to for the purposes of overlap testing.
        if (layerBounds.isEmpty())
            layerBounds.setSize(LayoutSize(1, 1));
        boundsComputed = true;
    }

    LayoutRect clipRect = layer->backgroundClipRect(rootRenderLayer(), 0, true).rect(); // FIXME: Incorrect for CSS regions.
    clipRect.scale(pageScaleFactor());
    clipRect.intersect(layerBounds);
    overlapMap.add(layer, clipRect);
}

void RenderLayerCompositor::addToOverlapMapRecursive(OverlapMap& overlapMap, RenderLayer* layer)
{
    if (!canBeComposited(layer) || overlapMap.contains(layer))
        return;

    LayoutRect bounds;
    bool haveComputedBounds = false;
    addToOverlapMap(overlapMap, layer, bounds, haveComputedBounds);

    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                addToOverlapMapRecursive(overlapMap, curLayer);
            }
        }
    }

    ASSERT(!layer->m_normalFlowListDirty);
    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            addToOverlapMapRecursive(overlapMap, curLayer);
        }
    }

    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                addToOverlapMapRecursive(overlapMap, curLayer);
            }
        }
    }
}

bool RenderLayerCompositor::overlapsCompositedLayers(OverlapMap& overlapMap, const LayoutRect& layerBounds)
{
    RenderLayerCompositor::OverlapMap::const_iterator end = overlapMap.end();
    for (RenderLayerCompositor::OverlapMap::const_iterator it = overlapMap.begin(); it != end; ++it) {
        const LayoutRect& bounds = it->second;
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
    LayoutRect absBounds;
    if (overlapMap && !overlapMap->isEmpty()) {
        // If we're testing for overlap, we only need to composite if we overlap something that is already composited.
        absBounds = layer->renderer()->localToAbsoluteQuad(FloatRect(layer->localBoundingBox())).enclosingBoundingBox();
        // Empty rects never intersect, but we need them to for the purposes of overlap testing.
        if (absBounds.isEmpty())
            absBounds.setSize(LayoutSize(1, 1));
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
    }

    if (overlapMap && childState.m_compositingAncestor && !childState.m_compositingAncestor->isRootLayer()) {
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
            addToOverlapMapRecursive(*overlapMap, layer);
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
            addToOverlapMapRecursive(*overlapMap, layer);
        willBeComposited = true;
    }

    // If we're back at the root, and no other layers need to be composited, and the root layer itself doesn't need
    // to be composited, then we can drop out of compositing mode altogether. However, don't drop out of compositing mode
    // if there are composited layers that we didn't hit in our traversal (e.g. because of visibility:hidden).
    if (layer->isRootLayer() && !childState.m_subtreeIsCompositing && !requiresCompositingLayer(layer) && !m_forceCompositingMode && !hasAnyAdditionalCompositedLayers(layer)) {
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

void RenderLayerCompositor::rebuildCompositingLayerTree(RenderLayer* layer, Vector<GraphicsLayer*>& childLayersOfEnclosingLayer)
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

    if (layer->isStackingContext()) {
        ASSERT(!layer->m_zOrderListsDirty);

        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                rebuildCompositingLayerTree(curLayer, childList);
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
            rebuildCompositingLayerTree(curLayer, childList);
        }
    }
    
    if (layer->isStackingContext()) {
        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                rebuildCompositingLayerTree(curLayer, childList);
            }
        }
    }
    
    if (layerBacking) {
        bool parented = false;
        if (layer->renderer()->isRenderPart())
            parented = parentFrameContentLayers(toRenderPart(layer->renderer()));

        if (!parented)
            layerBacking->parentForSublayers()->setChildren(layerChildren);

        // If the layer has a clipping layer the overflow controls layers will be siblings of the clipping layer.
        // Otherwise, the overflow control layers are normal children.
        if (!layerBacking->hasClippingLayer()) {
            if (GraphicsLayer* overflowControlLayer = layerBacking->layerForHorizontalScrollbar()) {
                overflowControlLayer->removeFromParent();
                layerBacking->parentForSublayers()->addChild(overflowControlLayer);
            }

            if (GraphicsLayer* overflowControlLayer = layerBacking->layerForVerticalScrollbar()) {
                overflowControlLayer->removeFromParent();
                layerBacking->parentForSublayers()->addChild(overflowControlLayer);
            }

            if (GraphicsLayer* overflowControlLayer = layerBacking->layerForScrollCorner()) {
                overflowControlLayer->removeFromParent();
                layerBacking->parentForSublayers()->addChild(overflowControlLayer);
            }
        }

#if ENABLE(FULLSCREEN_API)
        // For the sake of clients of the full screen renderer, don't reparent
        // the full screen layer out from under them if they're in the middle of
        // animating.
        if (layer->renderer()->isRenderFullScreen() && m_renderView->document()->isAnimatingFullScreen())
            return;
#endif
        childLayersOfEnclosingLayer.append(layerBacking->childForSuperlayers());
    }
}

void RenderLayerCompositor::frameViewDidChangeLocation(const LayoutPoint& contentsOffset)
{
    if (m_overflowControlsHostLayer)
        m_overflowControlsHostLayer->setPosition(contentsOffset);
}

void RenderLayerCompositor::frameViewDidChangeSize()
{
    if (m_clipLayer) {
        FrameView* frameView = m_renderView->frameView();
        m_clipLayer->setSize(frameView->visibleContentRect(false /* exclude scrollbars */).size());

        frameViewDidScroll();
        updateOverflowControlsLayers();

#if ENABLE(RUBBER_BANDING)
        if (m_layerForOverhangAreas)
            m_layerForOverhangAreas->setSize(frameView->frameRect().size());
#endif

#if ENABLE(THREADED_SCROLLING)
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->syncFrameViewGeometry(frameView);
#endif
    }
}

void RenderLayerCompositor::frameViewDidScroll()
{
    FrameView* frameView = m_renderView->frameView();
    LayoutPoint scrollPosition = frameView->scrollPosition();

    if (RenderLayerBacking* backing = rootRenderLayer()->backing())
        backing->graphicsLayer()->visibleRectChanged();

    if (m_scrollLayer)
        m_scrollLayer->setPosition(FloatPoint(-scrollPosition.x(), -scrollPosition.y()));
}

String RenderLayerCompositor::layerTreeAsText(bool showDebugInfo)
{
    updateCompositingLayers();

    if (!m_rootContentLayer)
        return String();

    // We skip dumping the scroll and clip layers to keep layerTreeAsText output
    // similar between platforms.
    return m_rootContentLayer->layerTreeAsText(showDebugInfo ? LayerTreeAsTextDebug : LayerTreeAsTextBehaviorNormal);
}

RenderLayerCompositor* RenderLayerCompositor::frameContentsCompositor(RenderPart* renderer)
{
    if (!renderer->node()->isFrameOwnerElement())
        return 0;
        
    HTMLFrameOwnerElement* element = static_cast<HTMLFrameOwnerElement*>(renderer->node());
    if (Document* contentDocument = element->contentDocument()) {
        if (RenderView* view = contentDocument->renderView())
            return view->compositor();
    }
    return 0;
}

bool RenderLayerCompositor::parentFrameContentLayers(RenderPart* renderer)
{
    RenderLayerCompositor* innerCompositor = frameContentsCompositor(renderer);
    if (!innerCompositor || !innerCompositor->inCompositingMode() || innerCompositor->rootLayerAttachment() != RootLayerAttachedViaEnclosingFrame)
        return false;
    
    RenderLayer* layer = renderer->layer();
    if (!layer->isComposited())
        return false;

    RenderLayerBacking* backing = layer->backing();
    GraphicsLayer* hostingLayer = backing->parentForSublayers();
    GraphicsLayer* rootLayer = innerCompositor->rootGraphicsLayer();
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


void RenderLayerCompositor::repaintCompositedLayersAbsoluteRect(const LayoutRect& absRect)
{
    recursiveRepaintLayerRect(rootRenderLayer(), absRect);
}

void RenderLayerCompositor::recursiveRepaintLayerRect(RenderLayer* layer, const LayoutRect& rect)
{
    // FIXME: This method does not work correctly with transforms.
    if (layer->isComposited())
        layer->setBackingNeedsRepaintInRect(rect);

    if (layer->hasCompositingDescendant()) {
        if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                LayoutRect childRect(rect);
                curLayer->convertToLayerCoords(layer, childRect);
                recursiveRepaintLayerRect(curLayer, childRect);
            }
        }

        if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                LayoutRect childRect(rect);
                curLayer->convertToLayerCoords(layer, childRect);
                recursiveRepaintLayerRect(curLayer, childRect);
            }
        }
    }
    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            LayoutRect childRect(rect);
            curLayer->convertToLayerCoords(layer, childRect);
            recursiveRepaintLayerRect(curLayer, childRect);
        }
    }
}

RenderLayer* RenderLayerCompositor::rootRenderLayer() const
{
    return m_renderView->layer();
}

GraphicsLayer* RenderLayerCompositor::rootGraphicsLayer() const
{
    if (m_overflowControlsHostLayer)
        return m_overflowControlsHostLayer.get();
    return m_rootContentLayer.get();
}

GraphicsLayer* RenderLayerCompositor::scrollLayer() const
{
    return m_scrollLayer.get();
}

void RenderLayerCompositor::didMoveOnscreen()
{
    if (!inCompositingMode() || m_rootLayerAttachment != RootLayerUnattached)
        return;

    RootLayerAttachment attachment = shouldPropagateCompositingToEnclosingFrame() ? RootLayerAttachedViaEnclosingFrame : RootLayerAttachedViaChromeClient;
    attachRootLayer(attachment);
}

void RenderLayerCompositor::willMoveOffscreen()
{
    if (!inCompositingMode() || m_rootLayerAttachment == RootLayerUnattached)
        return;

    detachRootLayer();
}

void RenderLayerCompositor::clearBackingForLayerIncludingDescendants(RenderLayer* layer)
{
    if (!layer)
        return;

    if (layer->isComposited())
        layer->clearBacking();
    
    for (RenderLayer* currLayer = layer->firstChild(); currLayer; currLayer = currLayer->nextSibling())
        clearBackingForLayerIncludingDescendants(currLayer);
}

void RenderLayerCompositor::clearBackingForAllLayers()
{
    clearBackingForLayerIncludingDescendants(m_renderView->layer());
}

void RenderLayerCompositor::updateRootLayerPosition()
{
    if (m_rootContentLayer) {
        const LayoutRect& documentRect = m_renderView->documentRect();
        m_rootContentLayer->setSize(documentRect.size());
        m_rootContentLayer->setPosition(documentRect.location());
    }
    if (m_clipLayer) {
        FrameView* frameView = m_renderView->frameView();
        m_clipLayer->setSize(frameView->visibleContentRect(false /* exclude scrollbars */).size());
    }

#if ENABLE(THREADED_SCROLLING)
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->syncFrameViewGeometry(m_renderView->frameView());
#endif
}

void RenderLayerCompositor::didStartAcceleratedAnimation(CSSPropertyID property)
{
    // If an accelerated animation or transition runs, we have to turn off overlap checking because
    // we don't do layout for every frame, but we have to ensure that the layering is
    // correct between the animating object and other objects on the page.
    if (property == CSSPropertyWebkitTransform)
        setCompositingConsultsOverlap(false);
}

bool RenderLayerCompositor::has3DContent() const
{
    return layerHas3DContent(rootRenderLayer());
}

bool RenderLayerCompositor::allowsIndependentlyCompositedFrames(const FrameView* view)
{
#if PLATFORM(MAC)
    // frames are only independently composited in Mac pre-WebKit2.
    return view->platformWidget();
#endif
    return false;
}

bool RenderLayerCompositor::shouldPropagateCompositingToEnclosingFrame() const
{
    // Parent document content needs to be able to render on top of a composited frame, so correct behavior
    // is to have the parent document become composited too. However, this can cause problems on platforms that
    // use native views for frames (like Mac), so disable that behavior on those platforms for now.
    HTMLFrameOwnerElement* ownerElement = enclosingFrameElement();
    RenderObject* renderer = ownerElement ? ownerElement->renderer() : 0;

    // If we are the top-level frame, don't propagate.
    if (!ownerElement)
        return false;

    if (!allowsIndependentlyCompositedFrames(m_renderView->frameView()))
        return true;

    if (!renderer || !renderer->isRenderPart())
        return false;

    // On Mac, only propagate compositing if the frame is overlapped in the parent
    // document, or the parent is already compositing, or the main frame is scaled.
    Frame* frame = m_renderView->frameView()->frame();
    Page* page = frame ? frame->page() : 0;
    if (page && page->pageScaleFactor() != 1)
        return true;
    
    RenderPart* frameRenderer = toRenderPart(renderer);
    if (frameRenderer->widget()) {
        ASSERT(frameRenderer->widget()->isFrameView());
        FrameView* view = static_cast<FrameView*>(frameRenderer->widget());
        if (view->isOverlappedIncludingAncestors() || view->hasCompositingAncestor())
            return true;
    }

    return false;
}

HTMLFrameOwnerElement* RenderLayerCompositor::enclosingFrameElement() const
{
    if (HTMLFrameOwnerElement* ownerElement = m_renderView->document()->ownerElement())
        return (ownerElement->hasTagName(iframeTag) || ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(objectTag)) ? ownerElement : 0;

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
             || requiresCompositingForFrame(renderer)
             || (canRender3DTransforms() && renderer->style()->backfaceVisibility() == BackfaceVisibilityHidden)
             || clipsCompositingDescendants(layer)
             || requiresCompositingForAnimation(renderer)
             || requiresCompositingForFullScreen(renderer)
             || requiresCompositingForFilters(renderer)
             || requiresCompositingForPosition(renderer, layer);
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

    return layer->backgroundClipRect(computeClipRoot, 0, true).rect() != PaintInfo::infiniteRect(); // FIXME: Incorrect for CSS regions.
}

// Return true if the given layer is a stacking context and has compositing child
// layers that it needs to clip. In this case we insert a clipping GraphicsLayer
// into the hierarchy between this layer and its children in the z-order hierarchy.
bool RenderLayerCompositor::clipsCompositingDescendants(const RenderLayer* layer) const
{
    return layer->hasCompositingDescendant() &&
           (layer->renderer()->hasOverflowClip() || layer->renderer()->hasClip());
}

bool RenderLayerCompositor::requiresCompositingForScrollableFrame() const
{
    // Need this done first to determine overflow.
    ASSERT(!m_renderView->needsLayout());

    ScrollView* scrollView = m_renderView->frameView();
    return scrollView->verticalScrollbar() || scrollView->horizontalScrollbar();
}

bool RenderLayerCompositor::requiresCompositingForTransform(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::ThreeDTransformTrigger))
        return false;

    RenderStyle* style = renderer->style();
    // Note that we ask the renderer if it has a transform, because the style may have transforms,
    // but the renderer may be an inline that doesn't suppport them.
    return renderer->hasTransform() && (style->transform().has3DOperation() || style->transformStyle3D() == TransformStyle3DPreserve3D || style->hasPerspective());
}

bool RenderLayerCompositor::requiresCompositingForVideo(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::VideoTrigger))
        return false;
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
    if (!(m_compositingTriggers & ChromeClient::CanvasTrigger))
        return false;

    if (renderer->isCanvas()) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(renderer->node());
        return canvas->renderingContext() && canvas->renderingContext()->isAccelerated();
    }
    return false;
}

bool RenderLayerCompositor::requiresCompositingForPlugin(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::PluginTrigger))
        return false;

    bool composite = (renderer->isEmbeddedObject() && toRenderEmbeddedObject(renderer)->allowsAcceleratedCompositing())
                  || (renderer->isApplet() && toRenderApplet(renderer)->allowsAcceleratedCompositing());
    if (!composite)
        return false;

    m_compositingDependsOnGeometry = true;
    
    RenderWidget* pluginRenderer = toRenderWidget(renderer);
    // If we can't reliably know the size of the plugin yet, don't change compositing state.
    if (pluginRenderer->needsLayout())
        return pluginRenderer->hasLayer() && pluginRenderer->layer()->isComposited();

    // Don't go into compositing mode if height or width are zero, or size is 1x1.
    LayoutRect contentBox = pluginRenderer->contentBoxRect();
    return contentBox.height() * contentBox.width() > 1;
}

bool RenderLayerCompositor::requiresCompositingForFrame(RenderObject* renderer) const
{
    if (!renderer->isRenderPart())
        return false;
    
    RenderPart* frameRenderer = toRenderPart(renderer);

    if (!frameRenderer->requiresAcceleratedCompositing())
        return false;

    m_compositingDependsOnGeometry = true;

    RenderLayerCompositor* innerCompositor = frameContentsCompositor(frameRenderer);
    if (!innerCompositor || !innerCompositor->shouldPropagateCompositingToEnclosingFrame())
        return false;

    // If we can't reliably know the size of the iframe yet, don't change compositing state.
    if (renderer->needsLayout())
        return frameRenderer->hasLayer() && frameRenderer->layer()->isComposited();
    
    // Don't go into compositing mode if height or width are zero.
    LayoutRect contentBox = frameRenderer->contentBoxRect();
    return contentBox.height() * contentBox.width() > 0;
}

bool RenderLayerCompositor::requiresCompositingForAnimation(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::AnimationTrigger))
        return false;

    if (AnimationController* animController = renderer->animation()) {
        return (animController->isRunningAnimationOnRenderer(renderer, CSSPropertyOpacity) && inCompositingMode())
            || animController->isRunningAnimationOnRenderer(renderer, CSSPropertyWebkitTransform);
    }
    return false;
}

bool RenderLayerCompositor::requiresCompositingWhenDescendantsAreCompositing(RenderObject* renderer) const
{
    return renderer->hasTransform() || renderer->isTransparent() || renderer->hasMask() || renderer->hasReflection() || renderer->hasFilter();
}
    
bool RenderLayerCompositor::requiresCompositingForFullScreen(RenderObject* renderer) const
{
#if ENABLE(FULLSCREEN_API)
    return renderer->isRenderFullScreen() && m_renderView->document()->isAnimatingFullScreen();
#else
    UNUSED_PARAM(renderer);
    return false;
#endif
}

bool RenderLayerCompositor::requiresCompositingForFilters(RenderObject* renderer) const
{
#if ENABLE(CSS_FILTERS)
    if (!(m_compositingTriggers & ChromeClient::FilterTrigger))
        return false;

    return renderer->hasFilter();
#else
    UNUSED_PARAM(renderer);
    return false;
#endif
}

bool RenderLayerCompositor::requiresCompositingForPosition(RenderObject* renderer, const RenderLayer* layer) const
{
    // position:fixed elements that create their own stacking context (e.g. have an explicit z-index,
    // opacity, transform) can get their own composited layer. A stacking context is required otherwise
    // z-index and clipping will be broken.
    if (!(renderer->isPositioned() && renderer->style()->position() == FixedPosition && layer->isStackingContext()))
        return false;

    if (Settings* settings = m_renderView->document()->settings())
        if (!settings->acceleratedCompositingForFixedPositionEnabled())
            return false;

    RenderObject* container = renderer->container();
    // If the renderer is not hooked up yet then we have to wait until it is.
    if (!container) {
        m_compositingNeedsUpdate = true;
        return false;
    }

    // Don't promote fixed position elements that are descendants of transformed elements.
    // They will stay fixed wrt the transformed element rather than the enclosing frame.
    if (container != m_renderView)
        return false;

    return true;
}

bool RenderLayerCompositor::hasNonIdentity3DTransform(RenderObject* renderer) const
{
    if (!renderer->hasTransform())
        return false;
    
    if (renderer->style()->hasPerspective())
        return true;

    if (TransformationMatrix* transform = toRenderBoxModelObject(renderer)->layer()->transform())
        return !transform->isAffine();
    
    return false;
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
    // This applies when the application UI handles scrolling, in which case RenderLayerCompositor doesn't need to manage it.
    if (m_renderView->frameView()->delegatesScrolling())
        return false;

    // We need to handle our own scrolling if we're:
    return !m_renderView->frameView()->platformWidget() // viewless (i.e. non-Mac, or Mac in WebKit2)
        || attachment == RootLayerAttachedViaEnclosingFrame; // a composited frame on Mac
}

static void paintScrollbar(Scrollbar* scrollbar, GraphicsContext& context, const LayoutRect& clip)
{
    if (!scrollbar)
        return;

    context.save();
    const LayoutRect& scrollbarRect = scrollbar->frameRect();
    context.translate(-scrollbarRect.x(), -scrollbarRect.y());
    LayoutRect transformedClip = clip;
    transformedClip.moveBy(scrollbarRect.location());
    scrollbar->paint(&context, transformedClip);
    context.restore();
}

void RenderLayerCompositor::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, GraphicsLayerPaintingPhase, const LayoutRect& clip)
{
    if (graphicsLayer == layerForHorizontalScrollbar())
        paintScrollbar(m_renderView->frameView()->horizontalScrollbar(), context, clip);
    else if (graphicsLayer == layerForVerticalScrollbar())
        paintScrollbar(m_renderView->frameView()->verticalScrollbar(), context, clip);
    else if (graphicsLayer == layerForScrollCorner()) {
        const LayoutRect& scrollCorner = m_renderView->frameView()->scrollCornerRect();
        context.save();
        context.translate(-scrollCorner.x(), -scrollCorner.y());
        LayoutRect transformedClip = clip;
        transformedClip.moveBy(scrollCorner.location());
        m_renderView->frameView()->paintScrollCorner(&context, transformedClip);
        context.restore();
#if PLATFORM(CHROMIUM) && ENABLE(RUBBER_BANDING)
    } else if (graphicsLayer == layerForOverhangAreas()) {
        ScrollView* view = m_renderView->frameView();
        view->calculateAndPaintOverhangAreas(&context, clip);
#endif
    }
}

bool RenderLayerCompositor::showDebugBorders(const GraphicsLayer* layer) const
{
    if (layer == m_layerForHorizontalScrollbar || layer == m_layerForVerticalScrollbar || layer == m_layerForScrollCorner)
        return m_showDebugBorders;

    return false;
}

bool RenderLayerCompositor::showRepaintCounter(const GraphicsLayer* layer) const
{
    if (layer == m_layerForHorizontalScrollbar || layer == m_layerForVerticalScrollbar || layer == m_layerForScrollCorner)
        return m_showDebugBorders;

    return false;
}

float RenderLayerCompositor::deviceScaleFactor() const
{
    Frame* frame = m_renderView->frameView()->frame();
    if (!frame)
        return 1;
    Page* page = frame->page();
    if (!page)
        return 1;
    return page->deviceScaleFactor();
}

float RenderLayerCompositor::pageScaleFactor() const
{
    Frame* frame = m_renderView->frameView()->frame();
    if (!frame)
        return 1;
    Page* page = frame->page();
    if (!page)
        return 1;
    return page->pageScaleFactor();
}

void RenderLayerCompositor::didCommitChangesForLayer(const GraphicsLayer*) const
{
    // Nothing to do here yet.
}

bool RenderLayerCompositor::keepLayersPixelAligned() const
{
    // When scaling, attempt to align compositing layers to pixel boundaries.
    return true;
}

static bool shouldCompositeOverflowControls(FrameView* view)
{
    if (view->platformWidget())
        return false;

#if ENABLE(THREADED_SCROLLING)
    if (Page* page = view->frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            return scrollingCoordinator->coordinatesScrollingForFrameView(view);
    }
#endif

#if !PLATFORM(CHROMIUM)
    if (!view->hasOverlayScrollbars())
        return false;
#endif
    return true;
}

bool RenderLayerCompositor::requiresHorizontalScrollbarLayer() const
{
    FrameView* view = m_renderView->frameView();
    return shouldCompositeOverflowControls(view) && view->horizontalScrollbar();
}

bool RenderLayerCompositor::requiresVerticalScrollbarLayer() const
{
    FrameView* view = m_renderView->frameView();
    return shouldCompositeOverflowControls(view) && view->verticalScrollbar();
}

bool RenderLayerCompositor::requiresScrollCornerLayer() const
{
    FrameView* view = m_renderView->frameView();
    return shouldCompositeOverflowControls(view) && view->isScrollCornerVisible();
}

#if ENABLE(RUBBER_BANDING)
bool RenderLayerCompositor::requiresOverhangAreasLayer() const
{
    // We don't want a layer if this is a subframe.
    if (m_renderView->document()->ownerElement())
        return false;

    // We do want a layer if we have a scrolling coordinator.
#if ENABLE(THREADED_SCROLLING)
    if (scrollingCoordinator())
        return true;
#endif

    // Chromium always wants a layer.
#if PLATFORM(CHROMIUM)
    return true;
#endif

    return false;
}
#endif

void RenderLayerCompositor::updateOverflowControlsLayers()
{
#if ENABLE(RUBBER_BANDING)
    if (requiresOverhangAreasLayer()) {
        if (!m_layerForOverhangAreas) {
            m_layerForOverhangAreas = GraphicsLayer::create(this);
#ifndef NDEBUG
            m_layerForOverhangAreas->setName("overhang areas");
#endif
            m_layerForOverhangAreas->setDrawsContent(false);
            m_layerForOverhangAreas->setSize(m_renderView->frameView()->frameRect().size());

            ScrollbarTheme::theme()->setUpOverhangAreasLayerContents(m_layerForOverhangAreas.get());

            // We want the overhang areas layer to be positioned below the frame contents,
            // so insert it below the clip layer.
            m_overflowControlsHostLayer->addChildBelow(m_layerForOverhangAreas.get(), m_clipLayer.get());
        }
    } else if (m_layerForOverhangAreas) {
        m_layerForOverhangAreas->removeFromParent();
        m_layerForOverhangAreas = nullptr;
    }
#endif

    if (requiresHorizontalScrollbarLayer()) {
        if (!m_layerForHorizontalScrollbar) {
            m_layerForHorizontalScrollbar = GraphicsLayer::create(this);
    #ifndef NDEBUG
            m_layerForHorizontalScrollbar->setName("horizontal scrollbar");
    #endif
            m_overflowControlsHostLayer->addChild(m_layerForHorizontalScrollbar.get());

#if ENABLE(THREADED_SCROLLING)
            if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->frameViewHorizontalScrollbarLayerDidChange(m_renderView->frameView(), m_layerForHorizontalScrollbar.get());
#endif
        }
    } else if (m_layerForHorizontalScrollbar) {
        m_layerForHorizontalScrollbar->removeFromParent();
        m_layerForHorizontalScrollbar = nullptr;

#if ENABLE(THREADED_SCROLLING)
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewHorizontalScrollbarLayerDidChange(m_renderView->frameView(), 0);
#endif
    }

    if (requiresVerticalScrollbarLayer()) {
        if (!m_layerForVerticalScrollbar) {
            m_layerForVerticalScrollbar = GraphicsLayer::create(this);
    #ifndef NDEBUG
            m_layerForVerticalScrollbar->setName("vertical scrollbar");
    #endif
            m_overflowControlsHostLayer->addChild(m_layerForVerticalScrollbar.get());

#if ENABLE(THREADED_SCROLLING)
            if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->frameViewVerticalScrollbarLayerDidChange(m_renderView->frameView(), m_layerForVerticalScrollbar.get());
#endif
        }
    } else if (m_layerForVerticalScrollbar) {
        m_layerForVerticalScrollbar->removeFromParent();
        m_layerForVerticalScrollbar = nullptr;

#if ENABLE(THREADED_SCROLLING)
        if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->frameViewVerticalScrollbarLayerDidChange(m_renderView->frameView(), 0);
#endif
    }

    if (requiresScrollCornerLayer()) {
        if (!m_layerForScrollCorner) {
            m_layerForScrollCorner = GraphicsLayer::create(this);
    #ifndef NDEBUG
            m_layerForScrollCorner->setName("scroll corner");
    #endif
            m_overflowControlsHostLayer->addChild(m_layerForScrollCorner.get());
        }
    } else if (m_layerForScrollCorner) {
        m_layerForScrollCorner->removeFromParent();
        m_layerForScrollCorner = nullptr;
    }

    m_renderView->frameView()->positionScrollbarLayers();
}

void RenderLayerCompositor::ensureRootLayer()
{
    RootLayerAttachment expectedAttachment = shouldPropagateCompositingToEnclosingFrame() ? RootLayerAttachedViaEnclosingFrame : RootLayerAttachedViaChromeClient;
    if (expectedAttachment == m_rootLayerAttachment)
         return;

    if (!m_rootContentLayer) {
        m_rootContentLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
        m_rootContentLayer->setName("content root");
#endif
        m_rootContentLayer->setSize(FloatSize(m_renderView->maxXLayoutOverflow(), m_renderView->maxYLayoutOverflow()));
        m_rootContentLayer->setPosition(FloatPoint());

        // Need to clip to prevent transformed content showing outside this frame
        m_rootContentLayer->setMasksToBounds(true);
    }

    if (requiresScrollLayer(expectedAttachment)) {
        if (!m_overflowControlsHostLayer) {
            ASSERT(!m_scrollLayer);
            ASSERT(!m_clipLayer);

            // Create a layer to host the clipping layer and the overflow controls layers.
            m_overflowControlsHostLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
            m_overflowControlsHostLayer->setName("overflow controls host");
#endif

            // Create a clipping layer if this is an iframe
            m_clipLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
            m_clipLayer->setName("frame clipping");
#endif
            m_clipLayer->setMasksToBounds(true);
            
            m_scrollLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
            m_scrollLayer->setName("frame scrolling");
#endif

            // Hook them up
            m_overflowControlsHostLayer->addChild(m_clipLayer.get());
            m_clipLayer->addChild(m_scrollLayer.get());
            m_scrollLayer->addChild(m_rootContentLayer.get());

            frameViewDidChangeSize();
            frameViewDidScroll();

#if ENABLE(THREADED_SCROLLING)
            if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->frameViewScrollLayerDidChange(m_renderView->frameView(), m_scrollLayer.get());
#endif
        }
    } else {
        if (m_overflowControlsHostLayer) {
            m_overflowControlsHostLayer = nullptr;
            m_clipLayer = nullptr;
            m_scrollLayer = nullptr;
        }
    }

    // Check to see if we have to change the attachment
    if (m_rootLayerAttachment != RootLayerUnattached)
        detachRootLayer();

    attachRootLayer(expectedAttachment);
}

void RenderLayerCompositor::destroyRootLayer()
{
    if (!m_rootContentLayer)
        return;

    detachRootLayer();

#if ENABLE(RUBBER_BANDING)
    if (m_layerForOverhangAreas) {
        m_layerForOverhangAreas->removeFromParent();
        m_layerForOverhangAreas = nullptr;
    }
#endif

    if (m_layerForHorizontalScrollbar) {
        m_layerForHorizontalScrollbar->removeFromParent();
        m_layerForHorizontalScrollbar = nullptr;
        if (Scrollbar* horizontalScrollbar = m_renderView->frameView()->verticalScrollbar())
            m_renderView->frameView()->invalidateScrollbar(horizontalScrollbar, LayoutRect(LayoutPoint(0, 0), horizontalScrollbar->frameRect().size()));
    }

    if (m_layerForVerticalScrollbar) {
        m_layerForVerticalScrollbar->removeFromParent();
        m_layerForVerticalScrollbar = nullptr;
        if (Scrollbar* verticalScrollbar = m_renderView->frameView()->verticalScrollbar())
            m_renderView->frameView()->invalidateScrollbar(verticalScrollbar, LayoutRect(LayoutPoint(0, 0), verticalScrollbar->frameRect().size()));
    }

    if (m_layerForScrollCorner) {
        m_layerForScrollCorner = nullptr;
        m_renderView->frameView()->invalidateScrollCorner(m_renderView->frameView()->scrollCornerRect());
    }

    if (m_overflowControlsHostLayer) {
        m_overflowControlsHostLayer = nullptr;
        m_clipLayer = nullptr;
        m_scrollLayer = nullptr;
    }
    ASSERT(!m_scrollLayer);
    m_rootContentLayer = nullptr;
}

void RenderLayerCompositor::attachRootLayer(RootLayerAttachment attachment)
{
    if (!m_rootContentLayer)
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

            page->chrome()->client()->attachRootGraphicsLayer(frame, rootGraphicsLayer());
            break;
        }
        case RootLayerAttachedViaEnclosingFrame: {
            // The layer will get hooked up via RenderLayerBacking::updateGraphicsLayerConfiguration()
            // for the frame's renderer in the parent document.
            m_renderView->document()->ownerElement()->scheduleSetNeedsStyleRecalc(SyntheticStyleChange);
            break;
        }
    }
    
    m_rootLayerAttachment = attachment;
    rootLayerAttachmentChanged();
}

void RenderLayerCompositor::detachRootLayer()
{
    if (!m_rootContentLayer || m_rootLayerAttachment == RootLayerUnattached)
        return;

    switch (m_rootLayerAttachment) {
    case RootLayerAttachedViaEnclosingFrame: {
        // The layer will get unhooked up via RenderLayerBacking::updateGraphicsLayerConfiguration()
        // for the frame's renderer in the parent document.
        if (m_overflowControlsHostLayer)
            m_overflowControlsHostLayer->removeFromParent();
        else
            m_rootContentLayer->removeFromParent();

        if (HTMLFrameOwnerElement* ownerElement = m_renderView->document()->ownerElement())
            ownerElement->scheduleSetNeedsStyleRecalc(SyntheticStyleChange);
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
    ensureRootLayer();
}

void RenderLayerCompositor::rootLayerAttachmentChanged()
{
    // The attachment can affect whether the RenderView layer's paintingGoesToWindow() behavior,
    // so call updateGraphicsLayerGeometry() to udpate that.
    RenderLayer* layer = m_renderView->layer();
    if (RenderLayerBacking* backing = layer ? layer->backing() : 0)
        backing->updateDrawsContent();
}

// IFrames are special, because we hook compositing layers together across iframe boundaries
// when both parent and iframe content are composited. So when this frame becomes composited, we have
// to use a synthetic style change to get the iframes into RenderLayers in order to allow them to composite.
void RenderLayerCompositor::notifyIFramesOfCompositingChange()
{
    Frame* frame = m_renderView->frameView() ? m_renderView->frameView()->frame() : 0;
    if (!frame)
        return;

    for (Frame* child = frame->tree()->firstChild(); child; child = child->tree()->traverseNext(frame)) {
        if (child->document() && child->document()->ownerElement())
            child->document()->ownerElement()->scheduleSetNeedsStyleRecalc(SyntheticStyleChange);
    }
    
    // Compositing also affects the answer to RenderIFrame::requiresAcceleratedCompositing(), so 
    // we need to schedule a style recalc in our parent document.
    if (HTMLFrameOwnerElement* ownerElement = m_renderView->document()->ownerElement())
        ownerElement->scheduleSetNeedsStyleRecalc(SyntheticStyleChange);
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

void RenderLayerCompositor::deviceOrPageScaleFactorChanged()
{
    // Start at the RenderView's layer, since that's where the scale is applied.
    RenderLayer* viewLayer = m_renderView->layer();
    if (!viewLayer->isComposited())
        return;

    if (GraphicsLayer* rootLayer = viewLayer->backing()->graphicsLayer())
        rootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
}

#if ENABLE(THREADED_SCROLLING)
ScrollingCoordinator* RenderLayerCompositor::scrollingCoordinator() const
{
    if (Frame* frame = m_renderView->frameView()->frame()) {
        if (Page* page = frame->page())
            return page->scrollingCoordinator();
    }

    return 0;
}
#endif

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
