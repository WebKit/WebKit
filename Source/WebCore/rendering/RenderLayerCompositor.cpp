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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "RenderLayerCompositor.h"

#include "CSSAnimationController.h"
#include "CSSPropertyNames.h"
#include "CanvasRenderingContext.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentTimeline.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "HTMLCanvasElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "NodeList.h"
#include "Page.h"
#include "PageOverlayController.h"
#include "RenderEmbeddedObject.h"
#include "RenderFragmentedFlow.h"
#include "RenderFullScreen.h"
#include "RenderGeometryMap.h"
#include "RenderIFrame.h"
#include "RenderLayerBacking.h"
#include "RenderReplica.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "ScrollingConstraints.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "TiledBacking.h"
#include "TransformState.h"
#include <wtf/MemoryPressureHandler.h>
#include <wtf/SetForScope.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "LegacyTileCache.h"
#include "RenderScrollbar.h"
#endif

#if PLATFORM(MAC)
#include "LocalDefaultSystemAppearance.h"
#endif

#if ENABLE(TREE_DEBUGGING)
#include "RenderTreeAsText.h"
#endif

#if ENABLE(3D_TRANSFORMS)
// This symbol is used to determine from a script whether 3D rendering is enabled (via 'nm').
WEBCORE_EXPORT bool WebCoreHas3DRendering = true;
#endif

#if !PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
#define USE_COMPOSITING_FOR_SMALL_CANVASES 1
#endif

namespace WebCore {

#if !USE(COMPOSITING_FOR_SMALL_CANVASES)
static const int canvasAreaThresholdRequiringCompositing = 50 * 100;
#endif
// During page loading delay layer flushes up to this many seconds to allow them coalesce, reducing workload.
#if PLATFORM(IOS_FAMILY)
static const Seconds throttledLayerFlushInitialDelay { 500_ms };
static const Seconds throttledLayerFlushDelay { 1.5_s };
#else
static const Seconds throttledLayerFlushInitialDelay { 500_ms };
static const Seconds throttledLayerFlushDelay { 500_ms };
#endif

using namespace HTMLNames;

class OverlapMapContainer {
public:
    void add(const LayoutRect& bounds)
    {
        m_layerRects.append(bounds);
        m_boundingBox.unite(bounds);
    }

    bool overlapsLayers(const LayoutRect& bounds) const
    {
        // Checking with the bounding box will quickly reject cases when
        // layers are created for lists of items going in one direction and
        // never overlap with each other.
        if (!bounds.intersects(m_boundingBox))
            return false;
        for (const auto& layerRect : m_layerRects) {
            if (layerRect.intersects(bounds))
                return true;
        }
        return false;
    }

    void unite(const OverlapMapContainer& otherContainer)
    {
        m_layerRects.appendVector(otherContainer.m_layerRects);
        m_boundingBox.unite(otherContainer.m_boundingBox);
    }
private:
    Vector<LayoutRect> m_layerRects;
    LayoutRect m_boundingBox;
};

class RenderLayerCompositor::OverlapMap {
    WTF_MAKE_NONCOPYABLE(OverlapMap);
public:
    OverlapMap()
        : m_geometryMap(UseTransforms)
    {
        // Begin assuming the root layer will be composited so that there is
        // something on the stack. The root layer should also never get an
        // popCompositingContainer call.
        pushCompositingContainer();
    }

    void add(const LayoutRect& bounds)
    {
        // Layers do not contribute to overlap immediately--instead, they will
        // contribute to overlap as soon as their composited ancestor has been
        // recursively processed and popped off the stack.
        ASSERT(m_overlapStack.size() >= 2);
        m_overlapStack[m_overlapStack.size() - 2].add(bounds);
        m_isEmpty = false;
    }

    bool overlapsLayers(const LayoutRect& bounds) const
    {
        return m_overlapStack.last().overlapsLayers(bounds);
    }

    bool isEmpty() const
    {
        return m_isEmpty;
    }

    void pushCompositingContainer()
    {
        m_overlapStack.append(OverlapMapContainer());
    }

    void popCompositingContainer()
    {
        m_overlapStack[m_overlapStack.size() - 2].unite(m_overlapStack.last());
        m_overlapStack.removeLast();
    }

    const RenderGeometryMap& geometryMap() const { return m_geometryMap; }
    RenderGeometryMap& geometryMap() { return m_geometryMap; }

private:
    struct RectList {
        Vector<LayoutRect> rects;
        LayoutRect boundingRect;
        
        void append(const LayoutRect& rect)
        {
            rects.append(rect);
            boundingRect.unite(rect);
        }

        void append(const RectList& rectList)
        {
            rects.appendVector(rectList.rects);
            boundingRect.unite(rectList.boundingRect);
        }
        
        bool intersects(const LayoutRect& rect) const
        {
            if (!rects.size() || !boundingRect.intersects(rect))
                return false;

            for (const auto& currentRect : rects) {
                if (currentRect.intersects(rect))
                    return true;
            }
            return false;
        }
    };

    Vector<OverlapMapContainer> m_overlapStack;
    RenderGeometryMap m_geometryMap;
    bool m_isEmpty { true };
};

struct RenderLayerCompositor::CompositingState {
    CompositingState(RenderLayer* compAncestor, bool testOverlap = true)
        : compositingAncestor(compAncestor)
        , subtreeIsCompositing(false)
        , testingOverlap(testOverlap)
        , ancestorHasTransformAnimation(false)
#if ENABLE(CSS_COMPOSITING)
        , hasNotIsolatedCompositedBlendingDescendants(false)
#endif
#if ENABLE(TREE_DEBUGGING)
        , depth(0)
#endif
    {
    }
    
    CompositingState(const CompositingState& other)
        : compositingAncestor(other.compositingAncestor)
        , subtreeIsCompositing(other.subtreeIsCompositing)
        , testingOverlap(other.testingOverlap)
        , ancestorHasTransformAnimation(other.ancestorHasTransformAnimation)
#if ENABLE(CSS_COMPOSITING)
        , hasNotIsolatedCompositedBlendingDescendants(other.hasNotIsolatedCompositedBlendingDescendants)
#endif
#if ENABLE(TREE_DEBUGGING)
        , depth(other.depth + 1)
#endif
    {
    }
    
    RenderLayer* compositingAncestor;
    bool subtreeIsCompositing;
    bool testingOverlap;
    bool ancestorHasTransformAnimation;
#if ENABLE(CSS_COMPOSITING)
    bool hasNotIsolatedCompositedBlendingDescendants;
#endif
#if ENABLE(TREE_DEBUGGING)
    int depth;
#endif
};

struct RenderLayerCompositor::OverlapExtent {
    LayoutRect bounds;
    bool extentComputed { false };
    bool hasTransformAnimation { false };
    bool animationCausesExtentUncertainty { false };

    bool knownToBeHaveExtentUncertainty() const { return extentComputed && animationCausesExtentUncertainty; }
};

#if !LOG_DISABLED
static inline bool compositingLogEnabled()
{
    return LogCompositing.state == WTFLogChannelOn;
}
#endif

RenderLayerCompositor::RenderLayerCompositor(RenderView& renderView)
    : m_renderView(renderView)
    , m_updateCompositingLayersTimer(*this, &RenderLayerCompositor::updateCompositingLayersTimerFired)
    , m_layerFlushTimer(*this, &RenderLayerCompositor::layerFlushTimerFired)
{
}

RenderLayerCompositor::~RenderLayerCompositor()
{
    // Take care that the owned GraphicsLayers are deleted first as their destructors may call back here.
    m_clipLayer = nullptr;
    m_scrollLayer = nullptr;
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
    auto& settings = m_renderView.settings();
    bool hasAcceleratedCompositing = settings.acceleratedCompositingEnabled();

    // We allow the chrome to override the settings, in case the page is rendered
    // on a chrome that doesn't allow accelerated compositing.
    if (hasAcceleratedCompositing) {
        m_compositingTriggers = page().chrome().client().allowedCompositingTriggers();
        hasAcceleratedCompositing = m_compositingTriggers;
    }

    bool showDebugBorders = settings.showDebugBorders();
    bool showRepaintCounter = settings.showRepaintCounter();
    bool acceleratedDrawingEnabled = settings.acceleratedDrawingEnabled();
    bool displayListDrawingEnabled = settings.displayListDrawingEnabled();

    // forceCompositingMode for subframes can only be computed after layout.
    bool forceCompositingMode = m_forceCompositingMode;
    if (isMainFrameCompositor())
        forceCompositingMode = m_renderView.settings().forceCompositingMode() && hasAcceleratedCompositing; 
    
    if (hasAcceleratedCompositing != m_hasAcceleratedCompositing || showDebugBorders != m_showDebugBorders || showRepaintCounter != m_showRepaintCounter || forceCompositingMode != m_forceCompositingMode) {
        setCompositingLayersNeedRebuild();
        m_layerNeedsCompositingUpdate = true;
    }

    bool debugBordersChanged = m_showDebugBorders != showDebugBorders;
    m_hasAcceleratedCompositing = hasAcceleratedCompositing;
    m_forceCompositingMode = forceCompositingMode;
    m_showDebugBorders = showDebugBorders;
    m_showRepaintCounter = showRepaintCounter;
    m_acceleratedDrawingEnabled = acceleratedDrawingEnabled;
    m_displayListDrawingEnabled = displayListDrawingEnabled;
    
    if (debugBordersChanged) {
        if (m_layerForHorizontalScrollbar)
            m_layerForHorizontalScrollbar->setShowDebugBorder(m_showDebugBorders);

        if (m_layerForVerticalScrollbar)
            m_layerForVerticalScrollbar->setShowDebugBorder(m_showDebugBorders);

        if (m_layerForScrollCorner)
            m_layerForScrollCorner->setShowDebugBorder(m_showDebugBorders);
    }
    
    if (updateCompositingPolicy())
        setCompositingLayersNeedRebuild();
}

void RenderLayerCompositor::cacheAcceleratedCompositingFlagsAfterLayout()
{
    cacheAcceleratedCompositingFlags();

    if (isMainFrameCompositor())
        return;

    bool forceCompositingMode = m_hasAcceleratedCompositing && m_renderView.settings().forceCompositingMode() && requiresCompositingForScrollableFrame();
    if (forceCompositingMode != m_forceCompositingMode) {
        m_forceCompositingMode = forceCompositingMode;
        setCompositingLayersNeedRebuild();
    }
}

bool RenderLayerCompositor::updateCompositingPolicy()
{
    if (!inCompositingMode())
        return false;

    auto currentPolicy = m_compositingPolicy;
    if (page().compositingPolicyOverride()) {
        m_compositingPolicy = page().compositingPolicyOverride().value();
        return m_compositingPolicy != currentPolicy;
    }
    
    auto memoryPolicy = MemoryPressureHandler::currentMemoryUsagePolicy();
    m_compositingPolicy = memoryPolicy == WTF::MemoryUsagePolicy::Unrestricted ? CompositingPolicy::Normal : CompositingPolicy::Conservative;
    return m_compositingPolicy != currentPolicy;
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

void RenderLayerCompositor::willRecalcStyle()
{
    m_layerNeedsCompositingUpdate = false;
    cacheAcceleratedCompositingFlags();
}

bool RenderLayerCompositor::didRecalcStyleWithNoPendingLayout()
{
    if (!m_layerNeedsCompositingUpdate)
        return false;
    
    return updateCompositingLayers(CompositingUpdateType::AfterStyleChange);
}

void RenderLayerCompositor::customPositionForVisibleRectComputation(const GraphicsLayer* graphicsLayer, FloatPoint& position) const
{
    if (graphicsLayer != m_scrollLayer.get())
        return;

    FloatPoint scrollPosition = -position;

    if (m_renderView.frameView().scrollBehaviorForFixedElements() == StickToDocumentBounds)
        scrollPosition = m_renderView.frameView().constrainScrollPositionForOverhang(roundedIntPoint(scrollPosition));

    position = -scrollPosition;
}

void RenderLayerCompositor::notifyFlushRequired(const GraphicsLayer* layer)
{
    scheduleLayerFlush(layer->canThrottleLayerFlush());
}

void RenderLayerCompositor::scheduleLayerFlushNow()
{
    m_hasPendingLayerFlush = false;
    page().chrome().client().scheduleCompositingLayerFlush();
}

void RenderLayerCompositor::scheduleLayerFlush(bool canThrottle)
{
    ASSERT(!m_flushingLayers);

    if (canThrottle)
        startInitialLayerFlushTimerIfNeeded();

    if (canThrottle && isThrottlingLayerFlushes()) {
        m_hasPendingLayerFlush = true;
        return;
    }
    scheduleLayerFlushNow();
}

FloatRect RenderLayerCompositor::visibleRectForLayerFlushing() const
{
    const FrameView& frameView = m_renderView.frameView();
#if PLATFORM(IOS_FAMILY)
    return frameView.exposedContentRect();
#else
    // Having a m_clipLayer indicates that we're doing scrolling via GraphicsLayers.
    FloatRect visibleRect = m_clipLayer ? FloatRect({ }, frameView.sizeForVisibleContent()) : frameView.visibleContentRect();

    if (frameView.viewExposedRect())
        visibleRect.intersect(frameView.viewExposedRect().value());

    return visibleRect;
#endif
}

void RenderLayerCompositor::flushPendingLayerChanges(bool isFlushRoot)
{
    // FrameView::flushCompositingStateIncludingSubframes() flushes each subframe,
    // but GraphicsLayer::flushCompositingState() will cross frame boundaries
    // if the GraphicsLayers are connected (the RootLayerAttachedViaEnclosingFrame case).
    // As long as we're not the root of the flush, we can bail.
    if (!isFlushRoot && rootLayerAttachment() == RootLayerAttachedViaEnclosingFrame)
        return;
    
    if (rootLayerAttachment() == RootLayerUnattached) {
#if PLATFORM(IOS_FAMILY)
        startLayerFlushTimerIfNeeded();
#endif
        m_shouldFlushOnReattach = true;
        return;
    }

    auto& frameView = m_renderView.frameView();
    AnimationUpdateBlock animationUpdateBlock(&frameView.frame().animation());

    ASSERT(!m_flushingLayers);
    {
        SetForScope<bool> flushingLayersScope(m_flushingLayers, true);

        if (auto* rootLayer = rootGraphicsLayer()) {
            FloatRect visibleRect = visibleRectForLayerFlushing();
            LOG_WITH_STREAM(Compositing,  stream << "\nRenderLayerCompositor " << this << " flushPendingLayerChanges (is root " << isFlushRoot << ") visible rect " << visibleRect);
            rootLayer->flushCompositingState(visibleRect);
        }
        
        ASSERT(m_flushingLayers);
    }

    updateScrollCoordinatedLayersAfterFlushIncludingSubframes();

#if PLATFORM(IOS_FAMILY)
    if (isFlushRoot)
        page().chrome().client().didFlushCompositingLayers();
#endif

    ++m_layerFlushCount;
    startLayerFlushTimerIfNeeded();
}

void RenderLayerCompositor::updateScrollCoordinatedLayersAfterFlushIncludingSubframes()
{
    updateScrollCoordinatedLayersAfterFlush();

    auto& frame = m_renderView.frameView().frame();
    for (Frame* subframe = frame.tree().firstChild(); subframe; subframe = subframe->tree().traverseNext(&frame)) {
        auto* view = subframe->contentRenderer();
        if (!view)
            continue;

        view->compositor().updateScrollCoordinatedLayersAfterFlush();
    }
}

void RenderLayerCompositor::updateScrollCoordinatedLayersAfterFlush()
{
    updateCustomLayersAfterFlush();

    HashSet<RenderLayer*> layersNeedingUpdate;
    std::swap(m_scrollCoordinatedLayersNeedingUpdate, layersNeedingUpdate);
    
    for (auto* layer : layersNeedingUpdate)
        updateScrollCoordinatedStatus(*layer, ScrollingNodeChangeFlags::Layer);
}

#if PLATFORM(IOS_FAMILY)
static bool scrollbarHasDisplayNone(Scrollbar* scrollbar)
{
    if (!scrollbar || !scrollbar->isCustomScrollbar())
        return false;

    std::unique_ptr<RenderStyle> scrollbarStyle = static_cast<RenderScrollbar*>(scrollbar)->getScrollbarPseudoStyle(ScrollbarBGPart, PseudoId::Scrollbar);
    return scrollbarStyle && scrollbarStyle->display() == DisplayType::None;
}

// FIXME: Can we make |layer| const RenderLayer&?
static void updateScrollingLayerWithClient(RenderLayer& layer, ChromeClient& client)
{
    auto* backing = layer.backing();
    ASSERT(backing);

    bool allowHorizontalScrollbar = !scrollbarHasDisplayNone(layer.horizontalScrollbar());
    bool allowVerticalScrollbar = !scrollbarHasDisplayNone(layer.verticalScrollbar());
    client.addOrUpdateScrollingLayer(layer.renderer().element(), backing->scrollingLayer()->platformLayer(), backing->scrollingContentsLayer()->platformLayer(),
        layer.scrollableContentsSize(), allowHorizontalScrollbar, allowVerticalScrollbar);
}
#endif

void RenderLayerCompositor::updateCustomLayersAfterFlush()
{
#if PLATFORM(IOS_FAMILY)
    registerAllViewportConstrainedLayers();

    if (!m_scrollingLayersNeedingUpdate.isEmpty()) {
        for (auto* layer : m_scrollingLayersNeedingUpdate)
            updateScrollingLayerWithClient(*layer, page().chrome().client());
        m_scrollingLayersNeedingUpdate.clear();
    }
    m_scrollingLayersNeedingUpdate.clear();
#endif
}

void RenderLayerCompositor::didFlushChangesForLayer(RenderLayer& layer, const GraphicsLayer* graphicsLayer)
{
    if (m_scrollCoordinatedLayers.contains(&layer))
        m_scrollCoordinatedLayersNeedingUpdate.add(&layer);

#if PLATFORM(IOS_FAMILY)
    if (m_scrollingLayers.contains(&layer))
        m_scrollingLayersNeedingUpdate.add(&layer);
#endif

    auto* backing = layer.backing();
    if (backing->backgroundLayerPaintsFixedRootBackground() && graphicsLayer == backing->backgroundLayer())
        fixedRootBackgroundLayerChanged();
}

void RenderLayerCompositor::didPaintBacking(RenderLayerBacking*)
{
    auto& frameView = m_renderView.frameView();
    frameView.setLastPaintTime(MonotonicTime::now());
    if (frameView.milestonesPendingPaint())
        frameView.firePaintRelatedMilestonesIfNeeded();
}

void RenderLayerCompositor::didChangeVisibleRect()
{
    auto* rootLayer = rootGraphicsLayer();
    if (!rootLayer)
        return;

    FloatRect visibleRect = visibleRectForLayerFlushing();
    bool requiresFlush = rootLayer->visibleRectChangeRequiresFlush(visibleRect);
    LOG_WITH_STREAM(Compositing, stream << "RenderLayerCompositor::didChangeVisibleRect " << visibleRect << " requiresFlush " << requiresFlush);
    if (requiresFlush)
        scheduleLayerFlushNow();
}

void RenderLayerCompositor::notifyFlushBeforeDisplayRefresh(const GraphicsLayer*)
{
    if (!m_layerUpdater) {
        PlatformDisplayID displayID = page().chrome().displayID();
        m_layerUpdater = std::make_unique<GraphicsLayerUpdater>(*this, displayID);
    }
    
    m_layerUpdater->scheduleUpdate();
}

void RenderLayerCompositor::flushLayersSoon(GraphicsLayerUpdater&)
{
    scheduleLayerFlush(true);
}

void RenderLayerCompositor::layerTiledBackingUsageChanged(const GraphicsLayer* graphicsLayer, bool usingTiledBacking)
{
    if (usingTiledBacking) {
        ++m_layersWithTiledBackingCount;
        graphicsLayer->tiledBacking()->setIsInWindow(page().isInWindow());
    } else {
        ASSERT(m_layersWithTiledBackingCount > 0);
        --m_layersWithTiledBackingCount;
    }
}

void RenderLayerCompositor::scheduleCompositingLayerUpdate()
{
    if (!m_updateCompositingLayersTimer.isActive())
        m_updateCompositingLayersTimer.startOneShot(0_s);
}

void RenderLayerCompositor::updateCompositingLayersTimerFired()
{
    updateCompositingLayers(CompositingUpdateType::AfterLayout);
}

bool RenderLayerCompositor::hasAnyAdditionalCompositedLayers(const RenderLayer& rootLayer) const
{
    int layerCount = m_compositedLayerCount + page().pageOverlayController().overlayCount();
    return layerCount > (rootLayer.isComposited() ? 1 : 0);
}

void RenderLayerCompositor::cancelCompositingLayerUpdate()
{
    m_updateCompositingLayersTimer.stop();
}

bool RenderLayerCompositor::updateCompositingLayers(CompositingUpdateType updateType, RenderLayer* updateRoot)
{
    LOG_WITH_STREAM(Compositing, stream << "RenderLayerCompositor " << this << " updateCompositingLayers " << updateType << " root " << updateRoot);

    if (updateType == CompositingUpdateType::AfterStyleChange || updateType == CompositingUpdateType::AfterLayout)
        cacheAcceleratedCompositingFlagsAfterLayout(); // Some flags (e.g. forceCompositingMode) depend on layout.

    m_updateCompositingLayersTimer.stop();

    ASSERT(m_renderView.document().pageCacheState() == Document::NotInPageCache);
    
    // Compositing layers will be updated in Document::setVisualUpdatesAllowed(bool) if suppressed here.
    if (!m_renderView.document().visualUpdatesAllowed())
        return false;

    // Avoid updating the layers with old values. Compositing layers will be updated after the layout is finished.
    if (m_renderView.needsLayout())
        return false;

    if (!m_compositing && (m_forceCompositingMode || (isMainFrameCompositor() && page().pageOverlayController().overlayCount())))
        enableCompositingMode(true);

    if (!m_reevaluateCompositingAfterLayout && !m_compositing)
        return false;

    ++m_compositingUpdateCount;

    AnimationUpdateBlock animationUpdateBlock(&m_renderView.frameView().frame().animation());

    SetForScope<bool> postLayoutChange(m_inPostLayoutUpdate, true);
    
    bool checkForHierarchyUpdate = m_reevaluateCompositingAfterLayout;
    bool needGeometryUpdate = false;
    bool needRootLayerConfigurationUpdate = m_rootLayerConfigurationNeedsUpdate;

    switch (updateType) {
    case CompositingUpdateType::AfterStyleChange:
    case CompositingUpdateType::AfterLayout:
    case CompositingUpdateType::OnHitTest:
        checkForHierarchyUpdate = true;
        break;
    case CompositingUpdateType::OnScroll:
    case CompositingUpdateType::OnCompositedScroll:
        checkForHierarchyUpdate = true; // Overlap can change with scrolling, so need to check for hierarchy updates.
        needGeometryUpdate = true;
        break;
    }

    if (!checkForHierarchyUpdate && !needGeometryUpdate && !needRootLayerConfigurationUpdate)
        return false;

    bool needHierarchyUpdate = m_compositingLayersNeedRebuild;
    bool isFullUpdate = !updateRoot;

    // Only clear the flag if we're updating the entire hierarchy.
    m_compositingLayersNeedRebuild = false;
    m_rootLayerConfigurationNeedsUpdate = false;
    updateRoot = &rootRenderLayer();

    if (isFullUpdate && updateType == CompositingUpdateType::AfterLayout)
        m_reevaluateCompositingAfterLayout = false;

    LOG(Compositing, " checkForHierarchyUpdate %d, needGeometryUpdate %d", checkForHierarchyUpdate, needHierarchyUpdate);

#if !LOG_DISABLED
    MonotonicTime startTime;
    if (compositingLogEnabled()) {
        ++m_rootLayerUpdateCount;
        startTime = MonotonicTime::now();
    }
#endif

    if (checkForHierarchyUpdate) {
        // Go through the layers in presentation order, so that we can compute which RenderLayers need compositing layers.
        // FIXME: we could maybe do this and the hierarchy udpate in one pass, but the parenting logic would be more complex.
        CompositingState compState(updateRoot);
        bool layersChanged = false;
        bool saw3DTransform = false;
        OverlapMap overlapTestRequestMap;
        computeCompositingRequirements(nullptr, *updateRoot, overlapTestRequestMap, compState, layersChanged, saw3DTransform);
        needHierarchyUpdate |= layersChanged;
    }

#if !LOG_DISABLED
    if (compositingLogEnabled() && isFullUpdate && (needHierarchyUpdate || needGeometryUpdate)) {
        m_obligateCompositedLayerCount = 0;
        m_secondaryCompositedLayerCount = 0;
        m_obligatoryBackingStoreBytes = 0;
        m_secondaryBackingStoreBytes = 0;

        auto& frame = m_renderView.frameView().frame();
        bool isMainFrame = isMainFrameCompositor();
        LOG_WITH_STREAM(Compositing, stream << "\nUpdate " << m_rootLayerUpdateCount << " of " << (isMainFrame ? "main frame" : frame.tree().uniqueName().string().utf8().data()) << " - compositing policy is " << m_compositingPolicy);
    }
#endif

    if (needHierarchyUpdate) {
        // Update the hierarchy of the compositing layers.
        Vector<Ref<GraphicsLayer>> childList;
        rebuildCompositingLayerTree(*updateRoot, childList, 0);

        // Host the document layer in the RenderView's root layer.
        if (isFullUpdate) {
            appendDocumentOverlayLayers(childList);
            // Even when childList is empty, don't drop out of compositing mode if there are
            // composited layers that we didn't hit in our traversal (e.g. because of visibility:hidden).
            if (childList.isEmpty() && !hasAnyAdditionalCompositedLayers(*updateRoot))
                destroyRootLayer();
            else if (m_rootContentLayer)
                m_rootContentLayer->setChildren(WTFMove(childList));
        }
        
        reattachSubframeScrollLayers();
    } else if (needGeometryUpdate) {
        // We just need to do a geometry update. This is only used for position:fixed scrolling;
        // most of the time, geometry is updated via RenderLayer::styleChanged().
        updateLayerTreeGeometry(*updateRoot, 0);
        ASSERT(!isFullUpdate || !m_subframeScrollLayersNeedReattach);
    } else if (needRootLayerConfigurationUpdate)
        m_renderView.layer()->backing()->updateConfiguration();
    
#if !LOG_DISABLED
    if (compositingLogEnabled() && isFullUpdate && (needHierarchyUpdate || needGeometryUpdate)) {
        MonotonicTime endTime = MonotonicTime::now();
        LOG(Compositing, "Total layers   primary   secondary   obligatory backing (KB)   secondary backing(KB)   total backing (KB)  update time (ms)\n");

        LOG(Compositing, "%8d %11d %9d %20.2f %22.2f %22.2f %18.2f\n",
            m_obligateCompositedLayerCount + m_secondaryCompositedLayerCount, m_obligateCompositedLayerCount,
            m_secondaryCompositedLayerCount, m_obligatoryBackingStoreBytes / 1024, m_secondaryBackingStoreBytes / 1024, (m_obligatoryBackingStoreBytes + m_secondaryBackingStoreBytes) / 1024, (endTime - startTime).milliseconds());
    }
#endif
    ASSERT(updateRoot || !m_compositingLayersNeedRebuild);

    if (!hasAcceleratedCompositing())
        enableCompositingMode(false);

    // Inform the inspector that the layer tree has changed.
    InspectorInstrumentation::layerTreeDidChange(&page());

    return true;
}

void RenderLayerCompositor::appendDocumentOverlayLayers(Vector<Ref<GraphicsLayer>>& childList)
{
    if (!isMainFrameCompositor() || !m_compositing)
        return;

    Ref<GraphicsLayer> overlayHost = page().pageOverlayController().layerWithDocumentOverlays();
    childList.append(WTFMove(overlayHost));
}

void RenderLayerCompositor::layerBecameNonComposited(const RenderLayer& layer)
{
    // Inform the inspector that the given RenderLayer was destroyed.
    InspectorInstrumentation::renderLayerDestroyed(&page(), layer);

    ASSERT(m_compositedLayerCount > 0);
    --m_compositedLayerCount;
}

#if !LOG_DISABLED
void RenderLayerCompositor::logLayerInfo(const RenderLayer& layer, int depth)
{
    if (!compositingLogEnabled())
        return;

    auto* backing = layer.backing();
    if (requiresCompositingLayer(layer) || layer.isRenderViewLayer()) {
        ++m_obligateCompositedLayerCount;
        m_obligatoryBackingStoreBytes += backing->backingStoreMemoryEstimate();
    } else {
        ++m_secondaryCompositedLayerCount;
        m_secondaryBackingStoreBytes += backing->backingStoreMemoryEstimate();
    }

    LayoutRect absoluteBounds = backing->compositedBounds();
    absoluteBounds.move(layer.offsetFromAncestor(m_renderView.layer()));
    
    StringBuilder logString;
    logString.append(String::format("%*p id %" PRIu64 " (%.3f,%.3f-%.3f,%.3f) %.2fKB", 12 + depth * 2, &layer, backing->graphicsLayer()->primaryLayerID(),
        absoluteBounds.x().toFloat(), absoluteBounds.y().toFloat(), absoluteBounds.maxX().toFloat(), absoluteBounds.maxY().toFloat(),
        backing->backingStoreMemoryEstimate() / 1024));
    
    if (!layer.renderer().style().hasAutoZIndex())
        logString.append(String::format(" z-index: %d", layer.renderer().style().zIndex())); 

    logString.appendLiteral(" (");
    logString.append(logReasonsForCompositing(layer));
    logString.appendLiteral(") ");

    if (backing->graphicsLayer()->contentsOpaque() || backing->paintsIntoCompositedAncestor() || backing->foregroundLayer() || backing->backgroundLayer()) {
        logString.append('[');
        bool prependSpace = false;
        if (backing->graphicsLayer()->contentsOpaque()) {
            logString.appendLiteral("opaque");
            prependSpace = true;
        }

        if (backing->paintsIntoCompositedAncestor()) {
            if (prependSpace)
                logString.appendLiteral(", ");
            logString.appendLiteral("paints into ancestor");
            prependSpace = true;
        }

        if (backing->foregroundLayer() || backing->backgroundLayer()) {
            if (prependSpace)
                logString.appendLiteral(", ");
            if (backing->foregroundLayer() && backing->backgroundLayer()) {
                logString.appendLiteral("+foreground+background");
                prependSpace = true;
            } else if (backing->foregroundLayer()) {
                logString.appendLiteral("+foreground");
                prependSpace = true;
            } else {
                logString.appendLiteral("+background");
                prependSpace = true;
            }
        }
        
        if (backing->paintsSubpixelAntialiasedText()) {
            if (prependSpace)
                logString.appendLiteral(", ");
            logString.appendLiteral("texty");
        }

        logString.appendLiteral("] ");
    }

    logString.append(layer.name());

    LOG(Compositing, "%s", logString.toString().utf8().data());
}
#endif

static bool checkIfDescendantClippingContextNeedsUpdate(const RenderLayer& layer, bool isClipping)
{
    for (auto* child = layer.firstChild(); child; child = child->nextSibling()) {
        auto* backing = child->backing();
        if (backing && (isClipping || backing->hasAncestorClippingLayer()))
            return true;

        if (checkIfDescendantClippingContextNeedsUpdate(*child, isClipping))
            return true;
    }
    return false;
}

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
static bool isScrollableOverflow(Overflow overflow)
{
    return overflow == Overflow::Scroll || overflow == Overflow::Auto;
}

static bool styleHasTouchScrolling(const RenderStyle& style)
{
    return style.useTouchOverflowScrolling() && (isScrollableOverflow(style.overflowX()) || isScrollableOverflow(style.overflowY()));
}
#endif

static bool styleChangeRequiresLayerRebuild(const RenderLayer& layer, const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    // Clip can affect ancestor compositing bounds, so we need recompute overlap when it changes on a non-composited layer.
    // FIXME: we should avoid doing this for all clip changes.
    if (oldStyle.clip() != newStyle.clip() || oldStyle.hasClip() != newStyle.hasClip())
        return true;

    // FIXME: need to check everything that we consult to avoid backing store here: webkit.org/b/138383
    if (!oldStyle.opacity() != !newStyle.opacity()) {
        auto* repaintContainer = layer.renderer().containerForRepaint();
        if (auto* ancestorBacking = repaintContainer ? repaintContainer->layer()->backing() : nullptr) {
            if (static_cast<bool>(newStyle.opacity()) != ancestorBacking->graphicsLayer()->drawsContent())
                return true;
        }
    }

    // When overflow changes, composited layers may need to update their ancestorClipping layers.
    if (!layer.isComposited() && (oldStyle.overflowX() != newStyle.overflowX() || oldStyle.overflowY() != newStyle.overflowY()) && layer.stackingContext()->hasCompositingDescendant())
        return true;
    
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    if (styleHasTouchScrolling(oldStyle) != styleHasTouchScrolling(newStyle))
        return true;
#endif

    // Compositing layers keep track of whether they are clipped by any of the ancestors.
    // When the current layer's clipping behaviour changes, we need to propagate it to the descendants.
    bool wasClipping = oldStyle.hasClip() || oldStyle.overflowX() != Overflow::Visible || oldStyle.overflowY() != Overflow::Visible;
    bool isClipping = newStyle.hasClip() || newStyle.overflowX() != Overflow::Visible || newStyle.overflowY() != Overflow::Visible;
    if (isClipping != wasClipping) {
        if (checkIfDescendantClippingContextNeedsUpdate(layer, isClipping))
            return true;
    }

    return false;
}

void RenderLayerCompositor::layerStyleChanged(StyleDifference diff, RenderLayer& layer, const RenderStyle* oldStyle)
{
    if (diff == StyleDifference::Equal)
        return;

    const RenderStyle& newStyle = layer.renderer().style();
    if (updateLayerCompositingState(layer) || (oldStyle && styleChangeRequiresLayerRebuild(layer, *oldStyle, newStyle))) {
        setCompositingLayersNeedRebuild();
        m_layerNeedsCompositingUpdate = true;
        return;
    }

    if (layer.isComposited()) {
        // FIXME: updating geometry here is potentially harmful, because layout is not up-to-date.
        layer.backing()->updateGeometry();
        layer.backing()->updateAfterDescendants();
        m_layerNeedsCompositingUpdate = true;
        return;
    }

    if (needsCompositingUpdateForStyleChangeOnNonCompositedLayer(layer, oldStyle))
        m_layerNeedsCompositingUpdate = true;
}

bool RenderLayerCompositor::needsCompositingUpdateForStyleChangeOnNonCompositedLayer(RenderLayer& layer, const RenderStyle* oldStyle) const
{
    // Needed for scroll bars.
    if (layer.isRenderViewLayer())
        return true;

    if (!oldStyle)
        return false;

    const RenderStyle& newStyle = layer.renderer().style();
    // Visibility change may affect geometry of the enclosing composited layer.
    if (oldStyle->visibility() != newStyle.visibility())
        return true;

    // We don't have any direct reasons for this style change to affect layer composition. Test if it might affect things indirectly.
    if (styleChangeMayAffectIndirectCompositingReasons(layer.renderer(), *oldStyle))
        return true;

    return false;
}

bool RenderLayerCompositor::canCompositeClipPath(const RenderLayer& layer)
{
    ASSERT(layer.isComposited());
    ASSERT(layer.renderer().style().clipPath());

    if (layer.renderer().hasMask())
        return false;

    auto& clipPath = *layer.renderer().style().clipPath();
    return (clipPath.type() != ClipPathOperation::Shape || clipPath.type() == ClipPathOperation::Shape) && GraphicsLayer::supportsLayerType(GraphicsLayer::Type::Shape);
}

static RenderLayerModelObject& rendererForCompositingTests(const RenderLayer& layer)
{
    auto* renderer = &layer.renderer();

    // The compositing state of a reflection should match that of its reflected layer.
    if (layer.isReflection())
        renderer = downcast<RenderLayerModelObject>(renderer->parent()); // The RenderReplica's parent is the object being reflected.

    return *renderer;
}

void RenderLayerCompositor::updateRootContentLayerClipping()
{
    m_rootContentLayer->setMasksToBounds(!m_renderView.settings().backgroundShouldExtendBeyondPage());
}

bool RenderLayerCompositor::updateBacking(RenderLayer& layer, CompositingChangeRepaint shouldRepaint, BackingRequired backingRequired)
{
    bool layerChanged = false;
    RenderLayer::ViewportConstrainedNotCompositedReason viewportConstrainedNotCompositedReason = RenderLayer::NoNotCompositedReason;

    if (backingRequired == BackingRequired::Unknown)
        backingRequired = needsToBeComposited(layer, &viewportConstrainedNotCompositedReason) ? BackingRequired::Yes : BackingRequired::No;
    else {
        // Need to fetch viewportConstrainedNotCompositedReason, but without doing all the work that needsToBeComposited does.
        requiresCompositingForPosition(rendererForCompositingTests(layer), layer, &viewportConstrainedNotCompositedReason);
    }

    if (backingRequired == BackingRequired::Yes) {
        enableCompositingMode();
        
        if (!layer.backing()) {
            // If we need to repaint, do so before making backing
            if (shouldRepaint == CompositingChangeRepaintNow)
                repaintOnCompositingChange(layer);

            layer.ensureBacking();

            if (layer.isRenderViewLayer() && useCoordinatedScrollingForLayer(layer)) {
                updateScrollCoordinatedStatus(layer, { ScrollingNodeChangeFlags::Layer, ScrollingNodeChangeFlags::LayerGeometry });
                if (auto* scrollingCoordinator = this->scrollingCoordinator())
                    scrollingCoordinator->frameViewRootLayerDidChange(m_renderView.frameView());
#if ENABLE(RUBBER_BANDING)
                updateLayerForHeader(page().headerHeight());
                updateLayerForFooter(page().footerHeight());
#endif
                updateRootContentLayerClipping();

                if (auto* tiledBacking = layer.backing()->tiledBacking())
                    tiledBacking->setTopContentInset(m_renderView.frameView().topContentInset());
            }

            // This layer and all of its descendants have cached repaints rects that are relative to
            // the repaint container, so change when compositing changes; we need to update them here.
            if (layer.parent())
                layer.computeRepaintRectsIncludingDescendants();

            layerChanged = true;
        }
    } else {
        if (layer.backing()) {
            // If we're removing backing on a reflection, clear the source GraphicsLayer's pointer to
            // its replica GraphicsLayer. In practice this should never happen because reflectee and reflection 
            // are both either composited, or not composited.
            if (layer.isReflection()) {
                auto* sourceLayer = downcast<RenderLayerModelObject>(*layer.renderer().parent()).layer();
                if (auto* backing = sourceLayer->backing()) {
                    ASSERT(backing->graphicsLayer()->replicaLayer() == layer.backing()->graphicsLayer());
                    backing->graphicsLayer()->setReplicatedByLayer(nullptr);
                }
            }

            removeFromScrollCoordinatedLayers(layer);

            layer.clearBacking();
            layerChanged = true;

            // This layer and all of its descendants have cached repaints rects that are relative to
            // the repaint container, so change when compositing changes; we need to update them here.
            layer.computeRepaintRectsIncludingDescendants();

            // If we need to repaint, do so now that we've removed the backing
            if (shouldRepaint == CompositingChangeRepaintNow)
                repaintOnCompositingChange(layer);
        }
    }
    
#if ENABLE(VIDEO)
    if (layerChanged && is<RenderVideo>(layer.renderer())) {
        // If it's a video, give the media player a chance to hook up to the layer.
        downcast<RenderVideo>(layer.renderer()).acceleratedRenderingStateChanged();
    }
#endif

    if (layerChanged && is<RenderWidget>(layer.renderer())) {
        auto* innerCompositor = frameContentsCompositor(&downcast<RenderWidget>(layer.renderer()));
        if (innerCompositor && innerCompositor->inCompositingMode())
            innerCompositor->updateRootLayerAttachment();
    }
    
    if (layerChanged)
        layer.clearClipRectsIncludingDescendants(PaintingClipRects);

    // If a fixed position layer gained/lost a backing or the reason not compositing it changed,
    // the scrolling coordinator needs to recalculate whether it can do fast scrolling.
    if (layer.renderer().isFixedPositioned()) {
        if (layer.viewportConstrainedNotCompositedReason() != viewportConstrainedNotCompositedReason) {
            layer.setViewportConstrainedNotCompositedReason(viewportConstrainedNotCompositedReason);
            layerChanged = true;
        }
        if (layerChanged) {
            if (auto* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->frameViewFixedObjectsDidChange(m_renderView.frameView());
        }
    } else
        layer.setViewportConstrainedNotCompositedReason(RenderLayer::NoNotCompositedReason);
    
    if (layer.backing())
        layer.backing()->updateDebugIndicators(m_showDebugBorders, m_showRepaintCounter);

    return layerChanged;
}

bool RenderLayerCompositor::updateLayerCompositingState(RenderLayer& layer, CompositingChangeRepaint shouldRepaint)
{
    bool layerChanged = updateBacking(layer, shouldRepaint);

    // See if we need content or clipping layers. Methods called here should assume
    // that the compositing state of descendant layers has not been updated yet.
    if (layer.backing() && layer.backing()->updateConfiguration())
        layerChanged = true;

    return layerChanged;
}

void RenderLayerCompositor::repaintOnCompositingChange(RenderLayer& layer)
{
    // If the renderer is not attached yet, no need to repaint.
    if (&layer.renderer() != &m_renderView && !layer.renderer().parent())
        return;

    auto* repaintContainer = layer.renderer().containerForRepaint();
    if (!repaintContainer)
        repaintContainer = &m_renderView;

    layer.repaintIncludingNonCompositingDescendants(repaintContainer);
    if (repaintContainer == &m_renderView) {
        // The contents of this layer may be moving between the window
        // and a GraphicsLayer, so we need to make sure the window system
        // synchronizes those changes on the screen.
        m_renderView.frameView().setNeedsOneShotDrawingSynchronization();
    }
}

// This method assumes that layout is up-to-date, unlike repaintOnCompositingChange().
void RenderLayerCompositor::repaintInCompositedAncestor(RenderLayer& layer, const LayoutRect& rect)
{
    auto* compositedAncestor = layer.enclosingCompositingLayerForRepaint(ExcludeSelf);
    if (!compositedAncestor)
        return;

    ASSERT(compositedAncestor->backing());
    LayoutRect repaintRect = rect;
    repaintRect.move(layer.offsetFromAncestor(compositedAncestor));
    compositedAncestor->setBackingNeedsRepaintInRect(repaintRect);

    // The contents of this layer may be moving from a GraphicsLayer to the window,
    // so we need to make sure the window system synchronizes those changes on the screen.
    if (compositedAncestor->isRenderViewLayer())
        m_renderView.frameView().setNeedsOneShotDrawingSynchronization();
}

void RenderLayerCompositor::layerWasAdded(RenderLayer&, RenderLayer&)
{
    setCompositingLayersNeedRebuild();
}

void RenderLayerCompositor::layerWillBeRemoved(RenderLayer& parent, RenderLayer& child)
{
    if (!child.isComposited() || parent.renderer().renderTreeBeingDestroyed())
        return;

    removeFromScrollCoordinatedLayers(child);
    repaintInCompositedAncestor(child, child.backing()->compositedBounds());

    setCompositingParent(child, nullptr);
    setCompositingLayersNeedRebuild();
}

RenderLayer* RenderLayerCompositor::enclosingNonStackingClippingLayer(const RenderLayer& layer) const
{
    for (auto* parent = layer.parent(); parent; parent = parent->parent()) {
        if (parent->isStackingContext())
            return nullptr;
        if (parent->renderer().hasClipOrOverflowClip())
            return parent;
    }
    return nullptr;
}

void RenderLayerCompositor::computeExtent(const OverlapMap& overlapMap, const RenderLayer& layer, OverlapExtent& extent) const
{
    if (extent.extentComputed)
        return;

    LayoutRect layerBounds;
    if (extent.hasTransformAnimation)
        extent.animationCausesExtentUncertainty = !layer.getOverlapBoundsIncludingChildrenAccountingForTransformAnimations(layerBounds);
    else
        layerBounds = layer.overlapBounds();
    
    // In the animating transform case, we avoid double-accounting for the transform because
    // we told pushMappingsToAncestor() to ignore transforms earlier.
    extent.bounds = enclosingLayoutRect(overlapMap.geometryMap().absoluteRect(layerBounds));

    // Empty rects never intersect, but we need them to for the purposes of overlap testing.
    if (extent.bounds.isEmpty())
        extent.bounds.setSize(LayoutSize(1, 1));


    RenderLayerModelObject& renderer = layer.renderer();
    if (renderer.isFixedPositioned() && renderer.container() == &m_renderView) {
        // Because fixed elements get moved around without re-computing overlap, we have to compute an overlap
        // rect that covers all the locations that the fixed element could move to.
        // FIXME: need to handle sticky too.
        extent.bounds = m_renderView.frameView().fixedScrollableAreaBoundsInflatedForScrolling(extent.bounds);
    }

    extent.extentComputed = true;
}

void RenderLayerCompositor::addToOverlapMap(OverlapMap& overlapMap, const RenderLayer& layer, OverlapExtent& extent)
{
    if (layer.isRenderViewLayer())
        return;

    computeExtent(overlapMap, layer, extent);

    LayoutRect clipRect = layer.backgroundClipRect(RenderLayer::ClipRectsContext(&rootRenderLayer(), AbsoluteClipRects)).rect(); // FIXME: Incorrect for CSS regions.

    // On iOS, pageScaleFactor() is not applied by RenderView, so we should not scale here.
    if (!m_renderView.settings().delegatesPageScaling())
        clipRect.scale(pageScaleFactor());
    clipRect.intersect(extent.bounds);
    overlapMap.add(clipRect);
}

void RenderLayerCompositor::addToOverlapMapRecursive(OverlapMap& overlapMap, const RenderLayer& layer, const RenderLayer* ancestorLayer)
{
    if (!canBeComposited(layer))
        return;

    // A null ancestorLayer is an indication that 'layer' has already been pushed.
    if (ancestorLayer)
        overlapMap.geometryMap().pushMappingsToAncestor(&layer, ancestorLayer);
    
    OverlapExtent layerExtent;
    addToOverlapMap(overlapMap, layer, layerExtent);

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer&>(layer));
#endif

    for (auto* renderLayer : layer.negativeZOrderLayers())
        addToOverlapMapRecursive(overlapMap, *renderLayer, &layer);

    for (auto* renderLayer : layer.normalFlowLayers())
        addToOverlapMapRecursive(overlapMap, *renderLayer, &layer);

    for (auto* renderLayer : layer.positiveZOrderLayers())
        addToOverlapMapRecursive(overlapMap, *renderLayer, &layer);
    
    if (ancestorLayer)
        overlapMap.geometryMap().popMappingsToAncestor(ancestorLayer);
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
void RenderLayerCompositor::computeCompositingRequirements(RenderLayer* ancestorLayer, RenderLayer& layer, OverlapMap& overlapMap, CompositingState& compositingState, bool& layersChanged, bool& descendantHas3DTransform)
{
    layer.updateDescendantDependentFlags();
    layer.updateLayerListsIfNeeded();

    // Clear the flag
    layer.setHasCompositingDescendant(false);
    layer.setIndirectCompositingReason(RenderLayer::IndirectCompositingReason::None);

    // Check if the layer needs to be composited for direct reasons (e.g. 3D transform).
    bool willBeComposited = needsToBeComposited(layer);

    OverlapExtent layerExtent;
    // Use the fact that we're composited as a hint to check for an animating transform.
    // FIXME: Maybe needsToBeComposited() should return a bitmask of reasons, to avoid the need to recompute things.
    if (willBeComposited && !layer.isRenderViewLayer())
        layerExtent.hasTransformAnimation = isRunningTransformAnimation(layer.renderer());

    bool respectTransforms = !layerExtent.hasTransformAnimation;
    overlapMap.geometryMap().pushMappingsToAncestor(&layer, ancestorLayer, respectTransforms);

    RenderLayer::IndirectCompositingReason compositingReason = compositingState.subtreeIsCompositing ? RenderLayer::IndirectCompositingReason::Stacking : RenderLayer::IndirectCompositingReason::None;

    // If we know for sure the layer is going to be composited, don't bother looking it up in the overlap map
    if (!willBeComposited && !overlapMap.isEmpty() && compositingState.testingOverlap) {
        computeExtent(overlapMap, layer, layerExtent);
        // If we're testing for overlap, we only need to composite if we overlap something that is already composited.
        compositingReason = overlapMap.overlapsLayers(layerExtent.bounds) ? RenderLayer::IndirectCompositingReason::Overlap : RenderLayer::IndirectCompositingReason::None;
    }

#if ENABLE(VIDEO)
    // Video is special. It's the only RenderLayer type that can both have
    // RenderLayer children and whose children can't use its backing to render
    // into. These children (the controls) always need to be promoted into their
    // own layers to draw on top of the accelerated video.
    if (compositingState.compositingAncestor && compositingState.compositingAncestor->renderer().isVideo())
        compositingReason = RenderLayer::IndirectCompositingReason::Overlap;
#endif

    layer.setIndirectCompositingReason(compositingReason);

    // Check if the computed indirect reason will force the layer to become composited.
    if (!willBeComposited && layer.mustCompositeForIndirectReasons() && canBeComposited(layer))
        willBeComposited = true;
    ASSERT(willBeComposited == needsToBeComposited(layer));

    // The children of this layer don't need to composite, unless there is
    // a compositing layer among them, so start by inheriting the compositing
    // ancestor with subtreeIsCompositing set to false.
    CompositingState childState(compositingState);
    childState.subtreeIsCompositing = false;
#if ENABLE(CSS_COMPOSITING)
    childState.hasNotIsolatedCompositedBlendingDescendants = false;
#endif

    if (willBeComposited) {
        // Tell the parent it has compositing descendants.
        compositingState.subtreeIsCompositing = true;
        // This layer now acts as the ancestor for kids.
        childState.compositingAncestor = &layer;

        overlapMap.pushCompositingContainer();
        // This layer is going to be composited, so children can safely ignore the fact that there's an 
        // animation running behind this layer, meaning they can rely on the overlap map testing again.
        childState.testingOverlap = true;

        computeExtent(overlapMap, layer, layerExtent);
        childState.ancestorHasTransformAnimation |= layerExtent.hasTransformAnimation;
        // Too hard to compute animated bounds if both us and some ancestor is animating transform.
        layerExtent.animationCausesExtentUncertainty |= layerExtent.hasTransformAnimation && compositingState.ancestorHasTransformAnimation;
    }

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer);
#endif

    bool anyDescendantHas3DTransform = false;

    for (auto* renderLayer : layer.negativeZOrderLayers()) {
        computeCompositingRequirements(&layer, *renderLayer, overlapMap, childState, layersChanged, anyDescendantHas3DTransform);

        // If we have to make a layer for this child, make one now so we can have a contents layer
        // (since we need to ensure that the -ve z-order child renders underneath our contents).
        if (!willBeComposited && childState.subtreeIsCompositing) {
            // make layer compositing
            layer.setIndirectCompositingReason(RenderLayer::IndirectCompositingReason::BackgroundLayer);
            childState.compositingAncestor = &layer;
            overlapMap.pushCompositingContainer();
            // This layer is going to be composited, so children can safely ignore the fact that there's an
            // animation running behind this layer, meaning they can rely on the overlap map testing again
            childState.testingOverlap = true;
            willBeComposited = true;
        }
    }
    
    for (auto* renderLayer : layer.normalFlowLayers())
        computeCompositingRequirements(&layer, *renderLayer, overlapMap, childState, layersChanged, anyDescendantHas3DTransform);

    for (auto* renderLayer : layer.positiveZOrderLayers())
        computeCompositingRequirements(&layer, *renderLayer, overlapMap, childState, layersChanged, anyDescendantHas3DTransform);

    // If we just entered compositing mode, the root will have become composited (as long as accelerated compositing is enabled).
    if (layer.isRenderViewLayer()) {
        if (inCompositingMode() && m_hasAcceleratedCompositing)
            willBeComposited = true;
    }
    
    ASSERT(willBeComposited == needsToBeComposited(layer));

    // All layers (even ones that aren't being composited) need to get added to
    // the overlap map. Layers that do not composite will draw into their
    // compositing ancestor's backing, and so are still considered for overlap.
    // FIXME: When layerExtent has taken animation bounds into account, we also know that the bounds
    // include descendants, so we don't need to add them all to the overlap map.
    if (childState.compositingAncestor && !childState.compositingAncestor->isRenderViewLayer())
        addToOverlapMap(overlapMap, layer, layerExtent);

#if ENABLE(CSS_COMPOSITING)
    layer.setHasNotIsolatedCompositedBlendingDescendants(childState.hasNotIsolatedCompositedBlendingDescendants);
    ASSERT(!layer.hasNotIsolatedCompositedBlendingDescendants() || layer.hasNotIsolatedBlendingDescendants());
#endif
    // Now check for reasons to become composited that depend on the state of descendant layers.
    RenderLayer::IndirectCompositingReason indirectCompositingReason;
    if (!willBeComposited && canBeComposited(layer)
        && requiresCompositingForIndirectReason(layer.renderer(), childState.subtreeIsCompositing, anyDescendantHas3DTransform, indirectCompositingReason)) {
        layer.setIndirectCompositingReason(indirectCompositingReason);
        childState.compositingAncestor = &layer;
        overlapMap.pushCompositingContainer();
        addToOverlapMapRecursive(overlapMap, layer);
        willBeComposited = true;
    }
    
    ASSERT(willBeComposited == needsToBeComposited(layer));
    if (layer.reflectionLayer()) {
        // FIXME: Shouldn't we call computeCompositingRequirements to handle a reflection overlapping with another renderer?
        layer.reflectionLayer()->setIndirectCompositingReason(willBeComposited ? RenderLayer::IndirectCompositingReason::Stacking : RenderLayer::IndirectCompositingReason::None);
    }

    // Subsequent layers in the parent stacking context also need to composite.
    if (childState.subtreeIsCompositing)
        compositingState.subtreeIsCompositing = true;

    // Set the flag to say that this layer has compositing children.
    layer.setHasCompositingDescendant(childState.subtreeIsCompositing);

    // setHasCompositingDescendant() may have changed the answer to needsToBeComposited() when clipping, so test that again.
    bool isCompositedClippingLayer = canBeComposited(layer) && clipsCompositingDescendants(layer);

    // Turn overlap testing off for later layers if it's already off, or if we have an animating transform.
    // Note that if the layer clips its descendants, there's no reason to propagate the child animation to the parent layers. That's because
    // we know for sure the animation is contained inside the clipping rectangle, which is already added to the overlap map.
    if ((!childState.testingOverlap && !isCompositedClippingLayer) || layerExtent.knownToBeHaveExtentUncertainty())
        compositingState.testingOverlap = false;
    
    if (isCompositedClippingLayer) {
        if (!willBeComposited) {
            childState.compositingAncestor = &layer;
            overlapMap.pushCompositingContainer();
            addToOverlapMapRecursive(overlapMap, layer);
            willBeComposited = true;
         }
    }

#if ENABLE(CSS_COMPOSITING)
    if ((willBeComposited && layer.hasBlendMode())
        || (layer.hasNotIsolatedCompositedBlendingDescendants() && !layer.isolatesCompositedBlending()))
        compositingState.hasNotIsolatedCompositedBlendingDescendants = true;
#endif

    if (childState.compositingAncestor == &layer && !layer.isRenderViewLayer())
        overlapMap.popCompositingContainer();

    // If we're back at the root, and no other layers need to be composited, and the root layer itself doesn't need
    // to be composited, then we can drop out of compositing mode altogether. However, don't drop out of compositing mode
    // if there are composited layers that we didn't hit in our traversal (e.g. because of visibility:hidden).
    if (layer.isRenderViewLayer() && !childState.subtreeIsCompositing && !requiresCompositingLayer(layer) && !m_forceCompositingMode && !hasAnyAdditionalCompositedLayers(layer)) {
        // Don't drop out of compositing on iOS, because we may flash. See <rdar://problem/8348337>.
#if !PLATFORM(IOS_FAMILY)
        enableCompositingMode(false);
        willBeComposited = false;
#endif
    }
    
    ASSERT(willBeComposited == needsToBeComposited(layer));

    // Update backing now, so that we can use isComposited() reliably during tree traversal in rebuildCompositingLayerTree().
    if (updateBacking(layer, CompositingChangeRepaintNow, willBeComposited ? BackingRequired::Yes : BackingRequired::No))
        layersChanged = true;

    if (layer.reflectionLayer() && updateLayerCompositingState(*layer.reflectionLayer(), CompositingChangeRepaintNow))
        layersChanged = true;

    descendantHas3DTransform |= anyDescendantHas3DTransform || layer.has3DTransform();

    overlapMap.geometryMap().popMappingsToAncestor(ancestorLayer);
}

void RenderLayerCompositor::setCompositingParent(RenderLayer& childLayer, RenderLayer* parentLayer)
{
    ASSERT(!parentLayer || childLayer.ancestorCompositingLayer() == parentLayer);
    ASSERT(childLayer.isComposited());

    // It's possible to be called with a parent that isn't yet composited when we're doing
    // partial updates as required by painting or hit testing. Just bail in that case;
    // we'll do a full layer update soon.
    if (!parentLayer || !parentLayer->isComposited())
        return;

    if (parentLayer) {
        auto* hostingLayer = parentLayer->backing()->parentForSublayers();
        auto* hostedLayer = childLayer.backing()->childForSuperlayers();
        
        hostingLayer->addChild(*hostedLayer);
    } else
        childLayer.backing()->childForSuperlayers()->removeFromParent();
}

void RenderLayerCompositor::removeCompositedChildren(RenderLayer& layer)
{
    ASSERT(layer.isComposited());

    layer.backing()->parentForSublayers()->removeAllChildren();
}

#if ENABLE(VIDEO)
bool RenderLayerCompositor::canAccelerateVideoRendering(RenderVideo& video) const
{
    if (!m_hasAcceleratedCompositing)
        return false;

    return video.supportsAcceleratedRendering();
}
#endif

void RenderLayerCompositor::rebuildCompositingLayerTree(RenderLayer& layer, Vector<Ref<GraphicsLayer>>& childLayersOfEnclosingLayer, int depth)
{
    // Make the layer compositing if necessary, and set up clipping and content layers.
    // Note that we can only do work here that is independent of whether the descendant layers
    // have been processed. computeCompositingRequirements() will already have done the repaint if necessary.

    auto* layerBacking = layer.backing();
    if (layerBacking) {
        // The compositing state of all our children has been updated already, so now
        // we can compute and cache the composited bounds for this layer.
        layerBacking->updateCompositedBounds();

        if (auto* reflection = layer.reflectionLayer()) {
            if (reflection->backing())
                reflection->backing()->updateCompositedBounds();
        }

        if (layerBacking->updateConfiguration())
            layerBacking->updateDebugIndicators(m_showDebugBorders, m_showRepaintCounter);
        
        layerBacking->updateGeometry();

        if (!layer.parent())
            updateRootLayerPosition();

#if !LOG_DISABLED
        logLayerInfo(layer, depth);
#else
        UNUSED_PARAM(depth);
#endif
    }

    // If this layer has backing, then we are collecting its children, otherwise appending
    // to the compositing child list of an enclosing layer.
    Vector<Ref<GraphicsLayer>> layerChildren;
    auto& childList = layerBacking ? layerChildren : childLayersOfEnclosingLayer;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer);
#endif

    for (auto* renderLayer : layer.negativeZOrderLayers())
        rebuildCompositingLayerTree(*renderLayer, childList, depth + 1);

    // If a negative z-order child is compositing, we get a foreground layer which needs to get parented.
    if (layer.negativeZOrderLayers().size()) {
        if (layerBacking && layerBacking->foregroundLayer())
            childList.append(*layerBacking->foregroundLayer());
    }
    
    for (auto* renderLayer : layer.normalFlowLayers())
        rebuildCompositingLayerTree(*renderLayer, childList, depth + 1);
    
    for (auto* renderLayer : layer.positiveZOrderLayers())
        rebuildCompositingLayerTree(*renderLayer, childList, depth + 1);

    if (layerBacking) {
        bool parented = false;
        if (is<RenderWidget>(layer.renderer()))
            parented = parentFrameContentLayers(&downcast<RenderWidget>(layer.renderer()));

        if (!parented)
            layerBacking->parentForSublayers()->setChildren(WTFMove(layerChildren));

        // If the layer has a clipping layer the overflow controls layers will be siblings of the clipping layer.
        // Otherwise, the overflow control layers are normal children.
        if (!layerBacking->hasClippingLayer() && !layerBacking->hasScrollingLayer()) {
            if (auto* overflowControlLayer = layerBacking->layerForHorizontalScrollbar())
                layerBacking->parentForSublayers()->addChild(*overflowControlLayer);

            if (auto* overflowControlLayer = layerBacking->layerForVerticalScrollbar())
                layerBacking->parentForSublayers()->addChild(*overflowControlLayer);

            if (auto* overflowControlLayer = layerBacking->layerForScrollCorner())
                layerBacking->parentForSublayers()->addChild(*overflowControlLayer);
        }

        childLayersOfEnclosingLayer.append(*layerBacking->childForSuperlayers());
    }
    
    if (auto* layerBacking = layer.backing())
        layerBacking->updateAfterDescendants();
}

void RenderLayerCompositor::frameViewDidChangeLocation(const IntPoint& contentsOffset)
{
    if (m_overflowControlsHostLayer)
        m_overflowControlsHostLayer->setPosition(contentsOffset);
}

void RenderLayerCompositor::frameViewDidChangeSize()
{
    if (m_clipLayer) {
        const FrameView& frameView = m_renderView.frameView();
        m_clipLayer->setSize(frameView.sizeForVisibleContent());
        m_clipLayer->setPosition(positionForClipLayer());

        frameViewDidScroll();
        updateOverflowControlsLayers();

#if ENABLE(RUBBER_BANDING)
        if (m_layerForOverhangAreas) {
            m_layerForOverhangAreas->setSize(frameView.frameRect().size());
            m_layerForOverhangAreas->setPosition(FloatPoint(0, m_renderView.frameView().topContentInset()));
        }
#endif
    }
}

bool RenderLayerCompositor::hasCoordinatedScrolling() const
{
    auto* scrollingCoordinator = this->scrollingCoordinator();
    return scrollingCoordinator && scrollingCoordinator->coordinatesScrollingForFrameView(m_renderView.frameView());
}

void RenderLayerCompositor::updateScrollLayerPosition()
{
    ASSERT(m_scrollLayer);

    auto& frameView = m_renderView.frameView();
    IntPoint scrollPosition = frameView.scrollPosition();

    m_scrollLayer->setPosition(FloatPoint(-scrollPosition.x(), -scrollPosition.y()));

    if (auto* fixedBackgroundLayer = fixedRootBackgroundLayer())
        fixedBackgroundLayer->setPosition(frameView.scrollPositionForFixedPosition());
}

FloatPoint RenderLayerCompositor::positionForClipLayer() const
{
    auto& frameView = m_renderView.frameView();

    return FloatPoint(
        frameView.shouldPlaceBlockDirectionScrollbarOnLeft() ? frameView.horizontalScrollbarIntrusion() : 0,
        FrameView::yPositionForInsetClipLayer(frameView.scrollPosition(), frameView.topContentInset()));
}

void RenderLayerCompositor::frameViewDidScroll()
{
    if (!m_scrollLayer)
        return;

    // If there's a scrolling coordinator that manages scrolling for this frame view,
    // it will also manage updating the scroll layer position.
    if (hasCoordinatedScrolling()) {
        // We have to schedule a flush in order for the main TiledBacking to update its tile coverage.
        scheduleLayerFlushNow();
        return;
    }

    updateScrollLayerPosition();
}

void RenderLayerCompositor::frameViewDidAddOrRemoveScrollbars()
{
    updateOverflowControlsLayers();
}

void RenderLayerCompositor::frameViewDidLayout()
{
    if (auto* renderViewBacking = m_renderView.layer()->backing())
        renderViewBacking->adjustTiledBackingCoverage();
}

void RenderLayerCompositor::rootFixedBackgroundsChanged()
{
    auto* renderViewBacking = m_renderView.layer()->backing();
    if (renderViewBacking && renderViewBacking->isFrameLayerWithTiledBacking())
        setCompositingLayersNeedRebuild();
}

void RenderLayerCompositor::scrollingLayerDidChange(RenderLayer& layer)
{
    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->scrollableAreaScrollLayerDidChange(layer);
}

void RenderLayerCompositor::fixedRootBackgroundLayerChanged()
{
    if (m_renderView.renderTreeBeingDestroyed())
        return;

    if (m_renderView.layer()->isComposited())
        updateScrollCoordinatedStatus(*m_renderView.layer(), ScrollingNodeChangeFlags::Layer);
}

String RenderLayerCompositor::layerTreeAsText(LayerTreeFlags flags)
{
    updateCompositingLayers(CompositingUpdateType::AfterLayout);

    if (!m_rootContentLayer)
        return String();

    flushPendingLayerChanges(true);

    LayerTreeAsTextBehavior layerTreeBehavior = LayerTreeAsTextBehaviorNormal;
    if (flags & LayerTreeFlagsIncludeDebugInfo)
        layerTreeBehavior |= LayerTreeAsTextDebug;
    if (flags & LayerTreeFlagsIncludeVisibleRects)
        layerTreeBehavior |= LayerTreeAsTextIncludeVisibleRects;
    if (flags & LayerTreeFlagsIncludeTileCaches)
        layerTreeBehavior |= LayerTreeAsTextIncludeTileCaches;
    if (flags & LayerTreeFlagsIncludeRepaintRects)
        layerTreeBehavior |= LayerTreeAsTextIncludeRepaintRects;
    if (flags & LayerTreeFlagsIncludePaintingPhases)
        layerTreeBehavior |= LayerTreeAsTextIncludePaintingPhases;
    if (flags & LayerTreeFlagsIncludeContentLayers)
        layerTreeBehavior |= LayerTreeAsTextIncludeContentLayers;
    if (flags & LayerTreeFlagsIncludeAcceleratesDrawing)
        layerTreeBehavior |= LayerTreeAsTextIncludeAcceleratesDrawing;
    if (flags & LayerTreeFlagsIncludeBackingStoreAttached)
        layerTreeBehavior |= LayerTreeAsTextIncludeBackingStoreAttached;

    // We skip dumping the scroll and clip layers to keep layerTreeAsText output
    // similar between platforms.
    String layerTreeText = m_rootContentLayer->layerTreeAsText(layerTreeBehavior);

    // Dump an empty layer tree only if the only composited layer is the main frame's tiled backing,
    // so that tests expecting us to drop out of accelerated compositing when there are no layers succeed.
    if (!hasAnyAdditionalCompositedLayers(rootRenderLayer()) && documentUsesTiledBacking() && !(layerTreeBehavior & LayerTreeAsTextIncludeTileCaches))
        layerTreeText = emptyString();

    // The true root layer is not included in the dump, so if we want to report
    // its repaint rects, they must be included here.
    if (flags & LayerTreeFlagsIncludeRepaintRects)
        return m_renderView.frameView().trackedRepaintRectsAsText() + layerTreeText;

    return layerTreeText;
}

RenderLayerCompositor* RenderLayerCompositor::frameContentsCompositor(RenderWidget* renderer)
{
    if (auto* contentDocument = renderer->frameOwnerElement().contentDocument()) {
        if (auto* view = contentDocument->renderView())
            return &view->compositor();
    }
    return nullptr;
}

bool RenderLayerCompositor::parentFrameContentLayers(RenderWidget* renderer)
{
    auto* innerCompositor = frameContentsCompositor(renderer);
    if (!innerCompositor || !innerCompositor->inCompositingMode() || innerCompositor->rootLayerAttachment() != RootLayerAttachedViaEnclosingFrame)
        return false;
    
    auto* layer = renderer->layer();
    if (!layer->isComposited())
        return false;

    auto* backing = layer->backing();
    auto* hostingLayer = backing->parentForSublayers();
    auto* rootLayer = innerCompositor->rootGraphicsLayer();
    if (hostingLayer->children().size() != 1 || hostingLayer->children()[0].ptr() != rootLayer) {
        hostingLayer->removeAllChildren();
        hostingLayer->addChild(*rootLayer);
    }
    return true;
}

// This just updates layer geometry without changing the hierarchy.
void RenderLayerCompositor::updateLayerTreeGeometry(RenderLayer& layer, int depth)
{
    if (auto* layerBacking = layer.backing()) {
        // The compositing state of all our children has been updated already, so now
        // we can compute and cache the composited bounds for this layer.
        layerBacking->updateCompositedBounds();

        if (auto* reflection = layer.reflectionLayer()) {
            if (reflection->backing())
                reflection->backing()->updateCompositedBounds();
        }

        layerBacking->updateConfiguration();
        layerBacking->updateGeometry();

        if (!layer.parent())
            updateRootLayerPosition();

#if !LOG_DISABLED
        logLayerInfo(layer, depth);
#else
        UNUSED_PARAM(depth);
#endif
    }

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer);
#endif

    for (auto* renderLayer : layer.negativeZOrderLayers())
        updateLayerTreeGeometry(*renderLayer, depth + 1);

    for (auto* renderLayer : layer.normalFlowLayers())
        updateLayerTreeGeometry(*renderLayer, depth + 1);
    
    for (auto* renderLayer : layer.positiveZOrderLayers())
        updateLayerTreeGeometry(*renderLayer, depth + 1);

    if (auto* layerBacking = layer.backing())
        layerBacking->updateAfterDescendants();
}

// Recurs down the RenderLayer tree until its finds the compositing descendants of compositingAncestor and updates their geometry.
void RenderLayerCompositor::updateCompositingDescendantGeometry(RenderLayer& compositingAncestor, RenderLayer& layer)
{
    if (&layer != &compositingAncestor) {
        if (auto* layerBacking = layer.backing()) {
            layerBacking->updateCompositedBounds();

            if (auto* reflection = layer.reflectionLayer()) {
                if (reflection->backing())
                    reflection->backing()->updateCompositedBounds();
            }

            layerBacking->updateGeometry();
            layerBacking->updateAfterDescendants();
            return;
        }
    }

    if (layer.reflectionLayer())
        updateCompositingDescendantGeometry(compositingAncestor, *layer.reflectionLayer());

    if (!layer.hasCompositingDescendant())
        return;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer);
#endif
    
    for (auto* renderLayer : layer.negativeZOrderLayers())
        updateCompositingDescendantGeometry(compositingAncestor, *renderLayer);

    for (auto* renderLayer : layer.normalFlowLayers())
        updateCompositingDescendantGeometry(compositingAncestor, *renderLayer);
    
    for (auto* renderLayer : layer.positiveZOrderLayers())
        updateCompositingDescendantGeometry(compositingAncestor, *renderLayer);
    
    if (&layer != &compositingAncestor) {
        if (auto* layerBacking = layer.backing())
            layerBacking->updateAfterDescendants();
    }
}

void RenderLayerCompositor::repaintCompositedLayers()
{
    recursiveRepaintLayer(rootRenderLayer());
}

void RenderLayerCompositor::recursiveRepaintLayer(RenderLayer& layer)
{
    // FIXME: This method does not work correctly with transforms.
    if (layer.isComposited() && !layer.backing()->paintsIntoCompositedAncestor())
        layer.setBackingNeedsRepaint();

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer);
#endif

    if (layer.hasCompositingDescendant()) {
        for (auto* renderLayer : layer.negativeZOrderLayers())
            recursiveRepaintLayer(*renderLayer);

        for (auto* renderLayer : layer.positiveZOrderLayers())
            recursiveRepaintLayer(*renderLayer);
    }

    for (auto* renderLayer : layer.normalFlowLayers())
        recursiveRepaintLayer(*renderLayer);
}

RenderLayer& RenderLayerCompositor::rootRenderLayer() const
{
    return *m_renderView.layer();
}

GraphicsLayer* RenderLayerCompositor::rootGraphicsLayer() const
{
    if (m_overflowControlsHostLayer)
        return m_overflowControlsHostLayer.get();
    return m_rootContentLayer.get();
}

void RenderLayerCompositor::setIsInWindow(bool isInWindow)
{
    LOG(Compositing, "RenderLayerCompositor %p setIsInWindow %d", this, isInWindow);

    if (!inCompositingMode())
        return;

    if (auto* rootLayer = rootGraphicsLayer()) {
        GraphicsLayer::traverse(*rootLayer, [isInWindow](GraphicsLayer& layer) {
            layer.setIsInWindow(isInWindow);
        });
    }

    if (isInWindow) {
        if (m_rootLayerAttachment != RootLayerUnattached)
            return;

        RootLayerAttachment attachment = isMainFrameCompositor() ? RootLayerAttachedViaChromeClient : RootLayerAttachedViaEnclosingFrame;
        attachRootLayer(attachment);
#if PLATFORM(IOS_FAMILY)
        registerAllViewportConstrainedLayers();
        registerAllScrollingLayers();
#endif
    } else {
        if (m_rootLayerAttachment == RootLayerUnattached)
            return;

        detachRootLayer();
#if PLATFORM(IOS_FAMILY)
        unregisterAllViewportConstrainedLayers();
        unregisterAllScrollingLayers();
#endif
    }
}

void RenderLayerCompositor::clearBackingForLayerIncludingDescendants(RenderLayer& layer)
{
    if (layer.isComposited()) {
        removeFromScrollCoordinatedLayers(layer);
        layer.clearBacking();
    }

    for (auto* childLayer = layer.firstChild(); childLayer; childLayer = childLayer->nextSibling())
        clearBackingForLayerIncludingDescendants(*childLayer);
}

void RenderLayerCompositor::clearBackingForAllLayers()
{
    clearBackingForLayerIncludingDescendants(*m_renderView.layer());
}

void RenderLayerCompositor::updateRootLayerPosition()
{
    if (m_rootContentLayer) {
        m_rootContentLayer->setSize(m_renderView.frameView().contentsSize());
        m_rootContentLayer->setPosition(m_renderView.frameView().positionForRootContentLayer());
        m_rootContentLayer->setAnchorPoint(FloatPoint3D());
    }
    if (m_clipLayer) {
        m_clipLayer->setSize(m_renderView.frameView().sizeForVisibleContent());
        m_clipLayer->setPosition(positionForClipLayer());
    }

#if ENABLE(RUBBER_BANDING)
    if (m_contentShadowLayer) {
        m_contentShadowLayer->setPosition(m_rootContentLayer->position());
        m_contentShadowLayer->setSize(m_rootContentLayer->size());
    }

    updateLayerForTopOverhangArea(m_layerForTopOverhangArea != nullptr);
    updateLayerForBottomOverhangArea(m_layerForBottomOverhangArea != nullptr);
    updateLayerForHeader(m_layerForHeader != nullptr);
    updateLayerForFooter(m_layerForFooter != nullptr);
#endif
}

bool RenderLayerCompositor::has3DContent() const
{
    return layerHas3DContent(rootRenderLayer());
}

bool RenderLayerCompositor::needsToBeComposited(const RenderLayer& layer, RenderLayer::ViewportConstrainedNotCompositedReason* viewportConstrainedNotCompositedReason) const
{
    if (!canBeComposited(layer))
        return false;

    return requiresCompositingLayer(layer, viewportConstrainedNotCompositedReason) || layer.mustCompositeForIndirectReasons() || (inCompositingMode() && layer.isRenderViewLayer());
}

// Note: this specifies whether the RL needs a compositing layer for intrinsic reasons.
// Use needsToBeComposited() to determine if a RL actually needs a compositing layer.
// static
bool RenderLayerCompositor::requiresCompositingLayer(const RenderLayer& layer, RenderLayer::ViewportConstrainedNotCompositedReason* viewportConstrainedNotCompositedReason) const
{
    auto& renderer = rendererForCompositingTests(layer);

    // The root layer always has a compositing layer, but it may not have backing.
    return requiresCompositingForTransform(renderer)
        || requiresCompositingForAnimation(renderer)
        || clipsCompositingDescendants(*renderer.layer())
        || requiresCompositingForPosition(renderer, *renderer.layer(), viewportConstrainedNotCompositedReason)
        || requiresCompositingForCanvas(renderer)
        || requiresCompositingForFilters(renderer)
        || requiresCompositingForWillChange(renderer)
        || requiresCompositingForBackfaceVisibility(renderer)
        || requiresCompositingForVideo(renderer)
        || requiresCompositingForFrame(renderer)
        || requiresCompositingForPlugin(renderer)
        || requiresCompositingForOverflowScrolling(*renderer.layer());
}

bool RenderLayerCompositor::canBeComposited(const RenderLayer& layer) const
{
    if (m_hasAcceleratedCompositing && layer.isSelfPaintingLayer()) {
        if (!layer.isInsideFragmentedFlow())
            return true;

        // CSS Regions flow threads do not need to be composited as we use composited RenderFragmentContainers
        // to render the background of the RenderFragmentedFlow.
        if (layer.isRenderFragmentedFlow())
            return false;

        return true;
    }
    return false;
}

#if ENABLE(FULLSCREEN_API)
enum class FullScreenDescendant { Yes, No, NotApplicable };
static FullScreenDescendant isDescendantOfFullScreenLayer(const RenderLayer& layer)
{
    auto& document = layer.renderer().document();

    if (!document.webkitIsFullScreen() || !document.fullScreenRenderer())
        return FullScreenDescendant::NotApplicable;

    auto* fullScreenLayer = document.fullScreenRenderer()->layer();
    if (!fullScreenLayer) {
        ASSERT_NOT_REACHED();
        return FullScreenDescendant::NotApplicable;
    }

    return layer.isDescendantOf(*fullScreenLayer) ? FullScreenDescendant::Yes : FullScreenDescendant::No;
}
#endif

bool RenderLayerCompositor::requiresOwnBackingStore(const RenderLayer& layer, const RenderLayer* compositingAncestorLayer, const LayoutRect& layerCompositedBoundsInAncestor, const LayoutRect& ancestorCompositedBounds) const
{
    auto& renderer = layer.renderer();

    if (compositingAncestorLayer
        && !(compositingAncestorLayer->backing()->graphicsLayer()->drawsContent()
            || compositingAncestorLayer->backing()->paintsIntoWindow()
            || compositingAncestorLayer->backing()->paintsIntoCompositedAncestor()))
        return true;

    if (layer.isRenderViewLayer()
        || layer.transform() // note: excludes perspective and transformStyle3D.
        || requiresCompositingForAnimation(renderer)
        || requiresCompositingForPosition(renderer, layer)
        || requiresCompositingForCanvas(renderer)
        || requiresCompositingForFilters(renderer)
        || requiresCompositingForWillChange(renderer)
        || requiresCompositingForBackfaceVisibility(renderer)
        || requiresCompositingForVideo(renderer)
        || requiresCompositingForFrame(renderer)
        || requiresCompositingForPlugin(renderer)
        || requiresCompositingForOverflowScrolling(layer)
        || renderer.isTransparent()
        || renderer.hasMask()
        || renderer.hasReflection()
        || renderer.hasFilter()
        || renderer.hasBackdropFilter())
        return true;

    if (layer.mustCompositeForIndirectReasons()) {
        RenderLayer::IndirectCompositingReason reason = layer.indirectCompositingReason();
        return reason == RenderLayer::IndirectCompositingReason::Overlap
            || reason == RenderLayer::IndirectCompositingReason::Stacking
            || reason == RenderLayer::IndirectCompositingReason::BackgroundLayer
            || reason == RenderLayer::IndirectCompositingReason::GraphicalEffect
            || reason == RenderLayer::IndirectCompositingReason::Preserve3D; // preserve-3d has to create backing store to ensure that 3d-transformed elements intersect.
    }

    if (!ancestorCompositedBounds.contains(layerCompositedBoundsInAncestor))
        return true;

    return false;
}

OptionSet<CompositingReason> RenderLayerCompositor::reasonsForCompositing(const RenderLayer& layer) const
{
    OptionSet<CompositingReason> reasons;

    if (!layer.isComposited())
        return reasons;

    auto& renderer = rendererForCompositingTests(layer);

    if (requiresCompositingForTransform(renderer))
        reasons.add(CompositingReason::Transform3D);

    if (requiresCompositingForVideo(renderer))
        reasons.add(CompositingReason::Video);
    else if (requiresCompositingForCanvas(renderer))
        reasons.add(CompositingReason::Canvas);
    else if (requiresCompositingForPlugin(renderer))
        reasons.add(CompositingReason::Plugin);
    else if (requiresCompositingForFrame(renderer))
        reasons.add(CompositingReason::IFrame);

    if ((canRender3DTransforms() && renderer.style().backfaceVisibility() == BackfaceVisibility::Hidden))
        reasons.add(CompositingReason::BackfaceVisibilityHidden);

    if (clipsCompositingDescendants(*renderer.layer()))
        reasons.add(CompositingReason::ClipsCompositingDescendants);

    if (requiresCompositingForAnimation(renderer))
        reasons.add(CompositingReason::Animation);

    if (requiresCompositingForFilters(renderer))
        reasons.add(CompositingReason::Filters);

    if (requiresCompositingForWillChange(renderer))
        reasons.add(CompositingReason::WillChange);

    if (requiresCompositingForPosition(renderer, *renderer.layer()))
        reasons.add(renderer.isFixedPositioned() ? CompositingReason::PositionFixed : CompositingReason::PositionSticky);

    if (requiresCompositingForOverflowScrolling(*renderer.layer()))
        reasons.add(CompositingReason::OverflowScrollingTouch);

    switch (renderer.layer()->indirectCompositingReason()) {
    case RenderLayer::IndirectCompositingReason::None:
        break;
    case RenderLayer::IndirectCompositingReason::Stacking:
        reasons.add(CompositingReason::Stacking);
        break;
    case RenderLayer::IndirectCompositingReason::Overlap:
        reasons.add(CompositingReason::Overlap);
        break;
    case RenderLayer::IndirectCompositingReason::BackgroundLayer:
        reasons.add(CompositingReason::NegativeZIndexChildren);
        break;
    case RenderLayer::IndirectCompositingReason::GraphicalEffect:
        if (renderer.hasTransform())
            reasons.add(CompositingReason::TransformWithCompositedDescendants);

        if (renderer.isTransparent())
            reasons.add(CompositingReason::OpacityWithCompositedDescendants);

        if (renderer.hasMask())
            reasons.add(CompositingReason::MaskWithCompositedDescendants);

        if (renderer.hasReflection())
            reasons.add(CompositingReason::ReflectionWithCompositedDescendants);

        if (renderer.hasFilter() || renderer.hasBackdropFilter())
            reasons.add(CompositingReason::FilterWithCompositedDescendants);

#if ENABLE(CSS_COMPOSITING)
        if (layer.isolatesCompositedBlending())
            reasons.add(CompositingReason::IsolatesCompositedBlendingDescendants);

        if (layer.hasBlendMode())
            reasons.add(CompositingReason::BlendingWithCompositedDescendants);
#endif
        break;
    case RenderLayer::IndirectCompositingReason::Perspective:
        reasons.add(CompositingReason::Perspective);
        break;
    case RenderLayer::IndirectCompositingReason::Preserve3D:
        reasons.add(CompositingReason::Preserve3D);
        break;
    }

    if (inCompositingMode() && renderer.layer()->isRenderViewLayer())
        reasons.add(CompositingReason::Root);

    return reasons;
}

#if !LOG_DISABLED
const char* RenderLayerCompositor::logReasonsForCompositing(const RenderLayer& layer)
{
    OptionSet<CompositingReason> reasons = reasonsForCompositing(layer);

    if (reasons & CompositingReason::Transform3D)
        return "3D transform";

    if (reasons & CompositingReason::Video)
        return "video";

    if (reasons & CompositingReason::Canvas)
        return "canvas";

    if (reasons & CompositingReason::Plugin)
        return "plugin";

    if (reasons & CompositingReason::IFrame)
        return "iframe";

    if (reasons & CompositingReason::BackfaceVisibilityHidden)
        return "backface-visibility: hidden";

    if (reasons & CompositingReason::ClipsCompositingDescendants)
        return "clips compositing descendants";

    if (reasons & CompositingReason::Animation)
        return "animation";

    if (reasons & CompositingReason::Filters)
        return "filters";

    if (reasons & CompositingReason::PositionFixed)
        return "position: fixed";

    if (reasons & CompositingReason::PositionSticky)
        return "position: sticky";

    if (reasons & CompositingReason::OverflowScrollingTouch)
        return "-webkit-overflow-scrolling: touch";

    if (reasons & CompositingReason::Stacking)
        return "stacking";

    if (reasons & CompositingReason::Overlap)
        return "overlap";

    if (reasons & CompositingReason::NegativeZIndexChildren)
        return "negative z-index children";

    if (reasons & CompositingReason::TransformWithCompositedDescendants)
        return "transform with composited descendants";

    if (reasons & CompositingReason::OpacityWithCompositedDescendants)
        return "opacity with composited descendants";

    if (reasons & CompositingReason::MaskWithCompositedDescendants)
        return "mask with composited descendants";

    if (reasons & CompositingReason::ReflectionWithCompositedDescendants)
        return "reflection with composited descendants";

    if (reasons & CompositingReason::FilterWithCompositedDescendants)
        return "filter with composited descendants";

#if ENABLE(CSS_COMPOSITING)
    if (reasons & CompositingReason::BlendingWithCompositedDescendants)
        return "blending with composited descendants";

    if (reasons & CompositingReason::IsolatesCompositedBlendingDescendants)
        return "isolates composited blending descendants";
#endif

    if (reasons & CompositingReason::Perspective)
        return "perspective";

    if (reasons & CompositingReason::Preserve3D)
        return "preserve-3d";

    if (reasons & CompositingReason::Root)
        return "root";

    return "";
}
#endif

// Return true if the given layer has some ancestor in the RenderLayer hierarchy that clips,
// up to the enclosing compositing ancestor. This is required because compositing layers are parented
// according to the z-order hierarchy, yet clipping goes down the renderer hierarchy.
// Thus, a RenderLayer can be clipped by a RenderLayer that is an ancestor in the renderer hierarchy,
// but a sibling in the z-order hierarchy.
bool RenderLayerCompositor::clippedByAncestor(RenderLayer& layer) const
{
    if (!layer.isComposited() || !layer.parent())
        return false;

    auto* compositingAncestor = layer.ancestorCompositingLayer();
    if (!compositingAncestor)
        return false;

    // If the compositingAncestor clips, that will be taken care of by clipsCompositingDescendants(),
    // so we only care about clipping between its first child that is our ancestor (the computeClipRoot),
    // and layer. The exception is when the compositingAncestor isolates composited blending children,
    // in this case it is not allowed to clipsCompositingDescendants() and each of its children
    // will be clippedByAncestor()s, including the compositingAncestor.
    auto* computeClipRoot = compositingAncestor;
    if (!compositingAncestor->isolatesCompositedBlending()) {
        computeClipRoot = nullptr;
        auto* parent = &layer;
        while (parent) {
            auto* next = parent->parent();
            if (next == compositingAncestor) {
                computeClipRoot = parent;
                break;
            }
            parent = next;
        }

        if (!computeClipRoot || computeClipRoot == &layer)
            return false;
    }

    return !layer.backgroundClipRect(RenderLayer::ClipRectsContext(computeClipRoot, TemporaryClipRects)).isInfinite(); // FIXME: Incorrect for CSS regions.
}

// Return true if the given layer is a stacking context and has compositing child
// layers that it needs to clip. In this case we insert a clipping GraphicsLayer
// into the hierarchy between this layer and its children in the z-order hierarchy.
bool RenderLayerCompositor::clipsCompositingDescendants(const RenderLayer& layer) const
{
    return layer.hasCompositingDescendant() && layer.renderer().hasClipOrOverflowClip() && !layer.isolatesCompositedBlending();
}

bool RenderLayerCompositor::requiresCompositingForScrollableFrame() const
{
    if (isMainFrameCompositor())
        return false;

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    if (!m_renderView.settings().asyncFrameScrollingEnabled())
        return false;
#endif

    if (!(m_compositingTriggers & ChromeClient::ScrollableNonMainFrameTrigger))
        return false;

    // Need this done first to determine overflow.
    ASSERT(!m_renderView.needsLayout());
    return m_renderView.frameView().isScrollable();
}

bool RenderLayerCompositor::requiresCompositingForTransform(RenderLayerModelObject& renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::ThreeDTransformTrigger))
        return false;

    // Note that we ask the renderer if it has a transform, because the style may have transforms,
    // but the renderer may be an inline that doesn't suppport them.
    if (!renderer.hasTransform())
        return false;
    
    switch (m_compositingPolicy) {
    case CompositingPolicy::Normal:
        return renderer.style().transform().has3DOperation();
    case CompositingPolicy::Conservative:
        // Continue to allow pages to avoid the very slow software filter path.
        if (renderer.style().transform().has3DOperation() && renderer.hasFilter())
            return true;
        return !renderer.style().transform().isRepresentableIn2D();
    }
    return false;
}

bool RenderLayerCompositor::requiresCompositingForBackfaceVisibility(RenderLayerModelObject& renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::ThreeDTransformTrigger))
        return false;

    if (renderer.style().backfaceVisibility() != BackfaceVisibility::Hidden)
        return false;

    if (renderer.layer()->has3DTransformedAncestor())
        return true;
    
    // FIXME: workaround for webkit.org/b/132801
    auto* stackingContext = renderer.layer()->stackingContext();
    if (stackingContext && stackingContext->renderer().style().transformStyle3D() == TransformStyle3D::Preserve3D)
        return true;

    return false;
}

bool RenderLayerCompositor::requiresCompositingForVideo(RenderLayerModelObject& renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::VideoTrigger))
        return false;

#if ENABLE(VIDEO)
    if (!is<RenderVideo>(renderer))
        return false;

    auto& video = downcast<RenderVideo>(renderer);
    return (video.requiresImmediateCompositing() || video.shouldDisplayVideo()) && canAccelerateVideoRendering(video);
#else
    UNUSED_PARAM(renderer);
    return false;
#endif
}

bool RenderLayerCompositor::requiresCompositingForCanvas(RenderLayerModelObject& renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::CanvasTrigger))
        return false;

    if (!renderer.isCanvas())
        return false;

    bool isCanvasLargeEnoughToForceCompositing = true;
#if !USE(COMPOSITING_FOR_SMALL_CANVASES)
    auto* canvas = downcast<HTMLCanvasElement>(renderer.element());
    auto canvasArea = canvas->size().area<RecordOverflow>();
    isCanvasLargeEnoughToForceCompositing = !canvasArea.hasOverflowed() && canvasArea.unsafeGet() >= canvasAreaThresholdRequiringCompositing;
#endif

    CanvasCompositingStrategy compositingStrategy = canvasCompositingStrategy(renderer);
    if (compositingStrategy == CanvasAsLayerContents)
        return true;

    if (m_compositingPolicy == CompositingPolicy::Normal)
        return compositingStrategy == CanvasPaintedToLayer && isCanvasLargeEnoughToForceCompositing;

    return false;
}

bool RenderLayerCompositor::requiresCompositingForPlugin(RenderLayerModelObject& renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::PluginTrigger))
        return false;

    bool isCompositedPlugin = is<RenderEmbeddedObject>(renderer) && downcast<RenderEmbeddedObject>(renderer).allowsAcceleratedCompositing();
    if (!isCompositedPlugin)
        return false;

    m_reevaluateCompositingAfterLayout = true;
    
    auto& pluginRenderer = downcast<RenderWidget>(renderer);
    if (pluginRenderer.style().visibility() != Visibility::Visible)
        return false;

    // If we can't reliably know the size of the plugin yet, don't change compositing state.
    if (pluginRenderer.needsLayout())
        return pluginRenderer.isComposited();

    // Don't go into compositing mode if height or width are zero, or size is 1x1.
    IntRect contentBox = snappedIntRect(pluginRenderer.contentBoxRect());
    return contentBox.height() * contentBox.width() > 1;
}

bool RenderLayerCompositor::requiresCompositingForFrame(RenderLayerModelObject& renderer) const
{
    if (!is<RenderWidget>(renderer))
        return false;

    auto& frameRenderer = downcast<RenderWidget>(renderer);
    if (frameRenderer.style().visibility() != Visibility::Visible)
        return false;

    if (!frameRenderer.requiresAcceleratedCompositing())
        return false;

    m_reevaluateCompositingAfterLayout = true;

    // If we can't reliably know the size of the iframe yet, don't change compositing state.
    if (!frameRenderer.parent() || frameRenderer.needsLayout())
        return frameRenderer.isComposited();
    
    // Don't go into compositing mode if height or width are zero.
    return !snappedIntRect(frameRenderer.contentBoxRect()).isEmpty();
}

bool RenderLayerCompositor::requiresCompositingForAnimation(RenderLayerModelObject& renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::AnimationTrigger))
        return false;

    if (auto* element = renderer.element()) {
        if (auto* timeline = element->document().existingTimeline()) {
            if (timeline->runningAnimationsForElementAreAllAccelerated(*element))
                return true;
        }
    }

    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled())
        return false;

    const AnimationBase::RunningState activeAnimationState = AnimationBase::Running | AnimationBase::Paused;
    auto& animController = renderer.animation();
    return (animController.isRunningAnimationOnRenderer(renderer, CSSPropertyOpacity, activeAnimationState)
            && (inCompositingMode() || (m_compositingTriggers & ChromeClient::AnimatedOpacityTrigger)))
            || animController.isRunningAnimationOnRenderer(renderer, CSSPropertyFilter, activeAnimationState)
#if ENABLE(FILTERS_LEVEL_2)
            || animController.isRunningAnimationOnRenderer(renderer, CSSPropertyWebkitBackdropFilter, activeAnimationState)
#endif
            || animController.isRunningAnimationOnRenderer(renderer, CSSPropertyTransform, activeAnimationState);
}

bool RenderLayerCompositor::requiresCompositingForIndirectReason(RenderLayerModelObject& renderer, bool hasCompositedDescendants, bool has3DTransformedDescendants, RenderLayer::IndirectCompositingReason& reason) const
{
    auto& layer = *downcast<RenderBoxModelObject>(renderer).layer();

    // When a layer has composited descendants, some effects, like 2d transforms, filters, masks etc must be implemented
    // via compositing so that they also apply to those composited descendants.
    if (hasCompositedDescendants && (layer.isolatesCompositedBlending() || layer.transform() || renderer.createsGroup() || renderer.hasReflection())) {
        reason = RenderLayer::IndirectCompositingReason::GraphicalEffect;
        return true;
    }

    // A layer with preserve-3d or perspective only needs to be composited if there are descendant layers that
    // will be affected by the preserve-3d or perspective.
    if (has3DTransformedDescendants) {
        if (renderer.style().transformStyle3D() == TransformStyle3D::Preserve3D) {
            reason = RenderLayer::IndirectCompositingReason::Preserve3D;
            return true;
        }
    
        if (renderer.style().hasPerspective()) {
            reason = RenderLayer::IndirectCompositingReason::Perspective;
            return true;
        }
    }

    reason = RenderLayer::IndirectCompositingReason::None;
    return false;
}

bool RenderLayerCompositor::styleChangeMayAffectIndirectCompositingReasons(const RenderLayerModelObject& renderer, const RenderStyle& oldStyle)
{
    auto& style = renderer.style();
    if (RenderElement::createsGroupForStyle(style) != RenderElement::createsGroupForStyle(oldStyle))
        return true;
    if (style.isolation() != oldStyle.isolation())
        return true;
    if (style.hasTransform() != oldStyle.hasTransform())
        return true;
    if (style.boxReflect() != oldStyle.boxReflect())
        return true;
    if (style.transformStyle3D() != oldStyle.transformStyle3D())
        return true;
    if (style.hasPerspective() != oldStyle.hasPerspective())
        return true;

    return false;
}

bool RenderLayerCompositor::requiresCompositingForFilters(RenderLayerModelObject& renderer) const
{
#if ENABLE(FILTERS_LEVEL_2)
    if (renderer.hasBackdropFilter())
        return true;
#endif

    if (!(m_compositingTriggers & ChromeClient::FilterTrigger))
        return false;

    return renderer.hasFilter();
}

bool RenderLayerCompositor::requiresCompositingForWillChange(RenderLayerModelObject& renderer) const
{
    if (!renderer.style().willChange() || !renderer.style().willChange()->canTriggerCompositing())
        return false;

#if ENABLE(FULLSCREEN_API)
    if (renderer.layer() && isDescendantOfFullScreenLayer(*renderer.layer()) == FullScreenDescendant::No)
        return false;
#endif

    if (m_compositingPolicy == CompositingPolicy::Conservative)
        return false;

    if (is<RenderBox>(renderer))
        return true;

    return renderer.style().willChange()->canTriggerCompositingOnInline();
}

bool RenderLayerCompositor::isAsyncScrollableStickyLayer(const RenderLayer& layer, const RenderLayer** enclosingAcceleratedOverflowLayer) const
{
    ASSERT(layer.renderer().isStickilyPositioned());

    auto* enclosingOverflowLayer = layer.enclosingOverflowClipLayer(ExcludeSelf);

#if PLATFORM(IOS_FAMILY)
    if (enclosingOverflowLayer && enclosingOverflowLayer->hasTouchScrollableOverflow()) {
        if (enclosingAcceleratedOverflowLayer)
            *enclosingAcceleratedOverflowLayer = enclosingOverflowLayer;
        return true;
    }
#else
    UNUSED_PARAM(enclosingAcceleratedOverflowLayer);
#endif
    // If the layer is inside normal overflow, it's not async-scrollable.
    if (enclosingOverflowLayer)
        return false;

    // No overflow ancestor, so see if the frame supports async scrolling.
    if (hasCoordinatedScrolling())
        return true;

#if PLATFORM(IOS_FAMILY)
    // iOS WK1 has fixed/sticky support in the main frame via WebFixedPositionContent.
    return isMainFrameCompositor();
#else
    return false;
#endif
}

bool RenderLayerCompositor::isViewportConstrainedFixedOrStickyLayer(const RenderLayer& layer) const
{
    if (layer.renderer().isStickilyPositioned())
        return isAsyncScrollableStickyLayer(layer);

    if (layer.renderer().style().position() != PositionType::Fixed)
        return false;

    // FIXME: Handle fixed inside of a transform, which should not behave as fixed.
    for (auto* stackingContext = layer.stackingContext(); stackingContext; stackingContext = stackingContext->stackingContext()) {
        if (stackingContext->isComposited() && stackingContext->renderer().isFixedPositioned())
            return false;
    }

    return true;
}

bool RenderLayerCompositor::useCoordinatedScrollingForLayer(const RenderLayer& layer) const
{
    if (layer.isRenderViewLayer() && hasCoordinatedScrolling())
        return true;

    return layer.hasTouchScrollableOverflow();
}

bool RenderLayerCompositor::requiresCompositingForPosition(RenderLayerModelObject& renderer, const RenderLayer& layer, RenderLayer::ViewportConstrainedNotCompositedReason* viewportConstrainedNotCompositedReason) const
{
    // position:fixed elements that create their own stacking context (e.g. have an explicit z-index,
    // opacity, transform) can get their own composited layer. A stacking context is required otherwise
    // z-index and clipping will be broken.
    if (!renderer.isPositioned())
        return false;
    
#if ENABLE(FULLSCREEN_API)
    if (isDescendantOfFullScreenLayer(layer) == FullScreenDescendant::No)
        return false;
#endif

    auto position = renderer.style().position();
    bool isFixed = renderer.isOutOfFlowPositioned() && position == PositionType::Fixed;
    if (isFixed && !layer.isStackingContext())
        return false;
    
    bool isSticky = renderer.isInFlowPositioned() && position == PositionType::Sticky;
    if (!isFixed && !isSticky)
        return false;

    // FIXME: acceleratedCompositingForFixedPositionEnabled should probably be renamed acceleratedCompositingForViewportConstrainedPositionEnabled().
    if (!m_renderView.settings().acceleratedCompositingForFixedPositionEnabled())
        return false;

    if (isSticky)
        return isAsyncScrollableStickyLayer(layer);

    auto container = renderer.container();
    // If the renderer is not hooked up yet then we have to wait until it is.
    if (!container) {
        m_reevaluateCompositingAfterLayout = true;
        return false;
    }

    // Don't promote fixed position elements that are descendants of a non-view container, e.g. transformed elements.
    // They will stay fixed wrt the container rather than the enclosing frame.
    if (container != &m_renderView) {
        if (viewportConstrainedNotCompositedReason)
            *viewportConstrainedNotCompositedReason = RenderLayer::NotCompositedForNonViewContainer;
        return false;
    }
    
    // Subsequent tests depend on layout. If we can't tell now, just keep things the way they are until layout is done.
    if (!m_inPostLayoutUpdate) {
        m_reevaluateCompositingAfterLayout = true;
        return layer.isComposited();
    }

    bool paintsContent = layer.isVisuallyNonEmpty() || layer.hasVisibleDescendant();
    if (!paintsContent) {
        if (viewportConstrainedNotCompositedReason)
            *viewportConstrainedNotCompositedReason = RenderLayer::NotCompositedForNoVisibleContent;
        return false;
    }

    // Fixed position elements that are invisible in the current view don't get their own layer.
    // FIXME: We shouldn't have to check useFixedLayout() here; one of the viewport rects needs to give the correct answer.
    LayoutRect viewBounds;
    if (m_renderView.frameView().useFixedLayout())
        viewBounds = m_renderView.unscaledDocumentRect();
    else
        viewBounds = m_renderView.frameView().rectForFixedPositionLayout();

    LayoutRect layerBounds = layer.calculateLayerBounds(&layer, LayoutSize(), { RenderLayer::UseLocalClipRectIfPossible, RenderLayer::IncludeLayerFilterOutsets, RenderLayer::UseFragmentBoxesExcludingCompositing,
        RenderLayer::ExcludeHiddenDescendants, RenderLayer::DontConstrainForMask, RenderLayer::IncludeCompositedDescendants });
    // Map to m_renderView to ignore page scale.
    FloatRect absoluteBounds = layer.renderer().localToContainerQuad(FloatRect(layerBounds), &m_renderView).boundingBox();
    if (!viewBounds.intersects(enclosingIntRect(absoluteBounds))) {
        if (viewportConstrainedNotCompositedReason)
            *viewportConstrainedNotCompositedReason = RenderLayer::NotCompositedForBoundsOutOfView;
        LOG_WITH_STREAM(Compositing, stream << "Layer " << &layer << " bounds " << absoluteBounds << " outside visible rect " << viewBounds);
        return false;
    }
    
    return true;
}

bool RenderLayerCompositor::requiresCompositingForOverflowScrolling(const RenderLayer& layer) const
{
#if PLATFORM(IOS_FAMILY)
    if (!layer.canUseAcceleratedTouchScrolling())
        return false;

    if (!m_inPostLayoutUpdate) {
        m_reevaluateCompositingAfterLayout = true;
        return layer.isComposited();
    }

    return layer.hasTouchScrollableOverflow();
#else
    UNUSED_PARAM(layer);
    return false;
#endif
}

bool RenderLayerCompositor::isRunningTransformAnimation(RenderLayerModelObject& renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::AnimationTrigger))
        return false;

    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
        if (auto* element = renderer.element()) {
            if (auto* timeline = element->document().existingTimeline())
                return timeline->isRunningAnimationOnRenderer(renderer, CSSPropertyTransform);
        }
        return false;
    }
    return renderer.animation().isRunningAnimationOnRenderer(renderer, CSSPropertyTransform, AnimationBase::Running | AnimationBase::Paused);
}

// If an element has negative z-index children, those children render in front of the 
// layer background, so we need an extra 'contents' layer for the foreground of the layer
// object.
bool RenderLayerCompositor::needsContentsCompositingLayer(const RenderLayer& layer) const
{
    return layer.hasNegativeZOrderLayers();
}

bool RenderLayerCompositor::requiresScrollLayer(RootLayerAttachment attachment) const
{
    auto& frameView = m_renderView.frameView();

    // This applies when the application UI handles scrolling, in which case RenderLayerCompositor doesn't need to manage it.
    if (frameView.delegatesScrolling() && isMainFrameCompositor())
        return false;

    // We need to handle our own scrolling if we're:
    return !m_renderView.frameView().platformWidget() // viewless (i.e. non-Mac, or Mac in WebKit2)
        || attachment == RootLayerAttachedViaEnclosingFrame; // a composited frame on Mac
}

void paintScrollbar(Scrollbar* scrollbar, GraphicsContext& context, const IntRect& clip)
{
    if (!scrollbar)
        return;

    context.save();
    const IntRect& scrollbarRect = scrollbar->frameRect();
    context.translate(-scrollbarRect.location());
    IntRect transformedClip = clip;
    transformedClip.moveBy(scrollbarRect.location());
    scrollbar->paint(context, transformedClip);
    context.restore();
}

void RenderLayerCompositor::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, GraphicsLayerPaintingPhase, const FloatRect& clip, GraphicsLayerPaintBehavior)
{
#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(m_renderView.document().useDarkAppearance());
#endif

    IntRect pixelSnappedRectForIntegralPositionedItems = snappedIntRect(LayoutRect(clip));
    if (graphicsLayer == layerForHorizontalScrollbar())
        paintScrollbar(m_renderView.frameView().horizontalScrollbar(), context, pixelSnappedRectForIntegralPositionedItems);
    else if (graphicsLayer == layerForVerticalScrollbar())
        paintScrollbar(m_renderView.frameView().verticalScrollbar(), context, pixelSnappedRectForIntegralPositionedItems);
    else if (graphicsLayer == layerForScrollCorner()) {
        const IntRect& scrollCorner = m_renderView.frameView().scrollCornerRect();
        context.save();
        context.translate(-scrollCorner.location());
        IntRect transformedClip = pixelSnappedRectForIntegralPositionedItems;
        transformedClip.moveBy(scrollCorner.location());
        m_renderView.frameView().paintScrollCorner(context, transformedClip);
        context.restore();
    }
}

bool RenderLayerCompositor::supportsFixedRootBackgroundCompositing() const
{
    auto* renderViewBacking = m_renderView.layer()->backing();
    return renderViewBacking && renderViewBacking->isFrameLayerWithTiledBacking();
}

bool RenderLayerCompositor::needsFixedRootBackgroundLayer(const RenderLayer& layer) const
{
    if (!layer.isRenderViewLayer())
        return false;

    if (m_renderView.settings().fixedBackgroundsPaintRelativeToDocument())
        return false;

    return supportsFixedRootBackgroundCompositing() && m_renderView.rootBackgroundIsEntirelyFixed();
}

GraphicsLayer* RenderLayerCompositor::fixedRootBackgroundLayer() const
{
    // Get the fixed root background from the RenderView layer's backing.
    auto* viewLayer = m_renderView.layer();
    if (!viewLayer)
        return nullptr;

    if (viewLayer->isComposited() && viewLayer->backing()->backgroundLayerPaintsFixedRootBackground())
        return viewLayer->backing()->backgroundLayer();

    return nullptr;
}

void RenderLayerCompositor::resetTrackedRepaintRects()
{
    if (auto* rootLayer = rootGraphicsLayer()) {
        GraphicsLayer::traverse(*rootLayer, [](GraphicsLayer& layer) {
            layer.resetTrackedRepaints();
        });
    }
}

float RenderLayerCompositor::deviceScaleFactor() const
{
    return m_renderView.document().deviceScaleFactor();
}

float RenderLayerCompositor::pageScaleFactor() const
{
    return page().pageScaleFactor();
}

float RenderLayerCompositor::zoomedOutPageScaleFactor() const
{
    return page().zoomedOutPageScaleFactor();
}

float RenderLayerCompositor::contentsScaleMultiplierForNewTiles(const GraphicsLayer*) const
{
#if PLATFORM(IOS_FAMILY)
    LegacyTileCache* tileCache = nullptr;
    if (auto* frameView = page().mainFrame().view())
        tileCache = frameView->legacyTileCache();

    if (!tileCache)
        return 1;

    return tileCache->tileControllerShouldUseLowScaleTiles() ? 0.125 : 1;
#else
    return 1;
#endif
}

void RenderLayerCompositor::didCommitChangesForLayer(const GraphicsLayer*) const
{
    // Nothing to do here yet.
}

bool RenderLayerCompositor::documentUsesTiledBacking() const
{
    auto* layer = m_renderView.layer();
    if (!layer)
        return false;

    auto* backing = layer->backing();
    if (!backing)
        return false;

    return backing->isFrameLayerWithTiledBacking();
}

bool RenderLayerCompositor::isMainFrameCompositor() const
{
    return m_renderView.frameView().frame().isMainFrame();
}

bool RenderLayerCompositor::shouldCompositeOverflowControls() const
{
    auto& frameView = m_renderView.frameView();

    if (frameView.platformWidget())
        return false;

    if (frameView.delegatesScrolling())
        return false;

    if (documentUsesTiledBacking())
        return true;

    if (m_overflowControlsHostLayer && isMainFrameCompositor())
        return true;

#if !USE(COORDINATED_GRAPHICS_THREADED)
    if (!frameView.hasOverlayScrollbars())
        return false;
#endif

    return true;
}

bool RenderLayerCompositor::requiresHorizontalScrollbarLayer() const
{
    return shouldCompositeOverflowControls() && m_renderView.frameView().horizontalScrollbar();
}

bool RenderLayerCompositor::requiresVerticalScrollbarLayer() const
{
    return shouldCompositeOverflowControls() && m_renderView.frameView().verticalScrollbar();
}

bool RenderLayerCompositor::requiresScrollCornerLayer() const
{
    return shouldCompositeOverflowControls() && m_renderView.frameView().isScrollCornerVisible();
}

#if ENABLE(RUBBER_BANDING)
bool RenderLayerCompositor::requiresOverhangAreasLayer() const
{
    if (!isMainFrameCompositor())
        return false;

    // We do want a layer if we're using tiled drawing and can scroll.
    if (documentUsesTiledBacking() && m_renderView.frameView().hasOpaqueBackground() && !m_renderView.frameView().prohibitsScrolling())
        return true;

    return false;
}

bool RenderLayerCompositor::requiresContentShadowLayer() const
{
    if (!isMainFrameCompositor())
        return false;

#if PLATFORM(COCOA)
    if (viewHasTransparentBackground())
        return false;

    // If the background is going to extend, then it doesn't make sense to have a shadow layer.
    if (m_renderView.settings().backgroundShouldExtendBeyondPage())
        return false;

    // On Mac, we want a content shadow layer if we're using tiled drawing and can scroll.
    if (documentUsesTiledBacking() && !m_renderView.frameView().prohibitsScrolling())
        return true;
#endif

    return false;
}

GraphicsLayer* RenderLayerCompositor::updateLayerForTopOverhangArea(bool wantsLayer)
{
    if (!isMainFrameCompositor())
        return nullptr;

    if (!wantsLayer) {
        GraphicsLayer::unparentAndClear(m_layerForTopOverhangArea);
        return nullptr;
    }

    if (!m_layerForTopOverhangArea) {
        m_layerForTopOverhangArea = GraphicsLayer::create(graphicsLayerFactory(), *this);
        m_layerForTopOverhangArea->setName("top overhang");
        m_scrollLayer->addChildBelow(*m_layerForTopOverhangArea, m_rootContentLayer.get());
    }

    return m_layerForTopOverhangArea.get();
}

GraphicsLayer* RenderLayerCompositor::updateLayerForBottomOverhangArea(bool wantsLayer)
{
    if (!isMainFrameCompositor())
        return nullptr;

    if (!wantsLayer) {
        GraphicsLayer::unparentAndClear(m_layerForBottomOverhangArea);
        return nullptr;
    }

    if (!m_layerForBottomOverhangArea) {
        m_layerForBottomOverhangArea = GraphicsLayer::create(graphicsLayerFactory(), *this);
        m_layerForBottomOverhangArea->setName("bottom overhang");
        m_scrollLayer->addChildBelow(*m_layerForBottomOverhangArea, m_rootContentLayer.get());
    }

    m_layerForBottomOverhangArea->setPosition(FloatPoint(0, m_rootContentLayer->size().height() + m_renderView.frameView().headerHeight()
        + m_renderView.frameView().footerHeight() + m_renderView.frameView().topContentInset()));
    return m_layerForBottomOverhangArea.get();
}

GraphicsLayer* RenderLayerCompositor::updateLayerForHeader(bool wantsLayer)
{
    if (!isMainFrameCompositor())
        return nullptr;

    if (!wantsLayer) {
        if (m_layerForHeader) {
            GraphicsLayer::unparentAndClear(m_layerForHeader);

            // The ScrollingTree knows about the header layer, and the position of the root layer is affected
            // by the header layer, so if we remove the header, we need to tell the scrolling tree.
            if (auto* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->frameViewRootLayerDidChange(m_renderView.frameView());
        }
        return nullptr;
    }

    if (!m_layerForHeader) {
        m_layerForHeader = GraphicsLayer::create(graphicsLayerFactory(), *this);
        m_layerForHeader->setName("header");
        m_scrollLayer->addChildAbove(*m_layerForHeader, m_rootContentLayer.get());
        m_renderView.frameView().addPaintPendingMilestones(DidFirstFlushForHeaderLayer);
    }

    m_layerForHeader->setPosition(FloatPoint(0,
        FrameView::yPositionForHeaderLayer(m_renderView.frameView().scrollPosition(), m_renderView.frameView().topContentInset())));
    m_layerForHeader->setAnchorPoint(FloatPoint3D());
    m_layerForHeader->setSize(FloatSize(m_renderView.frameView().visibleWidth(), m_renderView.frameView().headerHeight()));

    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->frameViewRootLayerDidChange(m_renderView.frameView());

    page().chrome().client().didAddHeaderLayer(*m_layerForHeader);

    return m_layerForHeader.get();
}

GraphicsLayer* RenderLayerCompositor::updateLayerForFooter(bool wantsLayer)
{
    if (!isMainFrameCompositor())
        return nullptr;

    if (!wantsLayer) {
        if (m_layerForFooter) {
            GraphicsLayer::unparentAndClear(m_layerForFooter);

            // The ScrollingTree knows about the footer layer, and the total scrollable size is affected
            // by the footer layer, so if we remove the footer, we need to tell the scrolling tree.
            if (auto* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->frameViewRootLayerDidChange(m_renderView.frameView());
        }
        return nullptr;
    }

    if (!m_layerForFooter) {
        m_layerForFooter = GraphicsLayer::create(graphicsLayerFactory(), *this);
        m_layerForFooter->setName("footer");
        m_scrollLayer->addChildAbove(*m_layerForFooter, m_rootContentLayer.get());
    }

    float totalContentHeight = m_rootContentLayer->size().height() + m_renderView.frameView().headerHeight() + m_renderView.frameView().footerHeight();
    m_layerForFooter->setPosition(FloatPoint(0, FrameView::yPositionForFooterLayer(m_renderView.frameView().scrollPosition(),
        m_renderView.frameView().topContentInset(), totalContentHeight, m_renderView.frameView().footerHeight())));
    m_layerForFooter->setAnchorPoint(FloatPoint3D());
    m_layerForFooter->setSize(FloatSize(m_renderView.frameView().visibleWidth(), m_renderView.frameView().footerHeight()));

    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->frameViewRootLayerDidChange(m_renderView.frameView());

    page().chrome().client().didAddFooterLayer(*m_layerForFooter);

    return m_layerForFooter.get();
}

#endif

bool RenderLayerCompositor::viewHasTransparentBackground(Color* backgroundColor) const
{
    if (m_renderView.frameView().isTransparent()) {
        if (backgroundColor)
            *backgroundColor = Color(); // Return an invalid color.
        return true;
    }

    Color documentBackgroundColor = m_renderView.frameView().documentBackgroundColor();
    if (!documentBackgroundColor.isValid())
        documentBackgroundColor = m_renderView.frameView().baseBackgroundColor();

    ASSERT(documentBackgroundColor.isValid());

    if (backgroundColor)
        *backgroundColor = documentBackgroundColor;

    return !documentBackgroundColor.isOpaque();
}

// We can't rely on getting layerStyleChanged() for a style change that affects the root background, because the style change may
// be on the body which has no RenderLayer.
void RenderLayerCompositor::rootOrBodyStyleChanged(RenderElement& renderer, const RenderStyle* oldStyle)
{
    if (!inCompositingMode())
        return;

    Color oldBackgroundColor;
    if (oldStyle)
        oldBackgroundColor = oldStyle->visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);

    if (oldBackgroundColor != renderer.style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor))
        rootBackgroundColorOrTransparencyChanged();

    bool hadFixedBackground = oldStyle && oldStyle->hasEntirelyFixedBackground();
    if (hadFixedBackground != renderer.style().hasEntirelyFixedBackground()) {
        setCompositingLayersNeedRebuild();
        scheduleCompositingLayerUpdate();
    }
}

void RenderLayerCompositor::rootBackgroundColorOrTransparencyChanged()
{
    if (!inCompositingMode())
        return;

    Color backgroundColor;
    bool isTransparent = viewHasTransparentBackground(&backgroundColor);
    
    Color extendedBackgroundColor = m_renderView.settings().backgroundShouldExtendBeyondPage() ? backgroundColor : Color();
    
    bool transparencyChanged = m_viewBackgroundIsTransparent != isTransparent;
    bool backgroundColorChanged = m_viewBackgroundColor != backgroundColor;
    bool extendedBackgroundColorChanged = m_rootExtendedBackgroundColor != extendedBackgroundColor;

    LOG(Compositing, "RenderLayerCompositor %p rootBackgroundColorOrTransparencyChanged. isTransparent=%d, transparencyChanged=%d, backgroundColorChanged=%d, extendedBackgroundColorChanged=%d", this, isTransparent, transparencyChanged, backgroundColorChanged, extendedBackgroundColorChanged);
    if (!transparencyChanged && !backgroundColorChanged && !extendedBackgroundColorChanged)
        return;

    m_viewBackgroundIsTransparent = isTransparent;
    m_viewBackgroundColor = backgroundColor;
    m_rootExtendedBackgroundColor = extendedBackgroundColor;
    
    if (extendedBackgroundColorChanged) {
        page().chrome().client().pageExtendedBackgroundColorDidChange(m_rootExtendedBackgroundColor);
        
#if ENABLE(RUBBER_BANDING)
        if (!m_layerForOverhangAreas)
            return;
        
        m_layerForOverhangAreas->setBackgroundColor(m_rootExtendedBackgroundColor);
        
        if (!m_rootExtendedBackgroundColor.isValid())
            m_layerForOverhangAreas->setCustomAppearance(GraphicsLayer::CustomAppearance::ScrollingOverhang);
#endif
    }
    
    setRootLayerConfigurationNeedsUpdate();
    scheduleCompositingLayerUpdate();
}

void RenderLayerCompositor::updateOverflowControlsLayers()
{
#if ENABLE(RUBBER_BANDING)
    if (requiresOverhangAreasLayer()) {
        if (!m_layerForOverhangAreas) {
            m_layerForOverhangAreas = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_layerForOverhangAreas->setName("overhang areas");
            m_layerForOverhangAreas->setDrawsContent(false);

            float topContentInset = m_renderView.frameView().topContentInset();
            IntSize overhangAreaSize = m_renderView.frameView().frameRect().size();
            overhangAreaSize.setHeight(overhangAreaSize.height() - topContentInset);
            m_layerForOverhangAreas->setSize(overhangAreaSize);
            m_layerForOverhangAreas->setPosition(FloatPoint(0, topContentInset));
            m_layerForOverhangAreas->setAnchorPoint(FloatPoint3D());

            if (m_renderView.settings().backgroundShouldExtendBeyondPage())
                m_layerForOverhangAreas->setBackgroundColor(m_renderView.frameView().documentBackgroundColor());
            else
                m_layerForOverhangAreas->setCustomAppearance(GraphicsLayer::CustomAppearance::ScrollingOverhang);

            // We want the overhang areas layer to be positioned below the frame contents,
            // so insert it below the clip layer.
            m_overflowControlsHostLayer->addChildBelow(*m_layerForOverhangAreas, m_clipLayer.get());
        }
    } else
        GraphicsLayer::unparentAndClear(m_layerForOverhangAreas);

    if (requiresContentShadowLayer()) {
        if (!m_contentShadowLayer) {
            m_contentShadowLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_contentShadowLayer->setName("content shadow");
            m_contentShadowLayer->setSize(m_rootContentLayer->size());
            m_contentShadowLayer->setPosition(m_rootContentLayer->position());
            m_contentShadowLayer->setAnchorPoint(FloatPoint3D());
            m_contentShadowLayer->setCustomAppearance(GraphicsLayer::CustomAppearance::ScrollingShadow);

            m_scrollLayer->addChildBelow(*m_contentShadowLayer, m_rootContentLayer.get());
        }
    } else
        GraphicsLayer::unparentAndClear(m_contentShadowLayer);
#endif

    if (requiresHorizontalScrollbarLayer()) {
        if (!m_layerForHorizontalScrollbar) {
            m_layerForHorizontalScrollbar = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_layerForHorizontalScrollbar->setCanDetachBackingStore(false);
            m_layerForHorizontalScrollbar->setShowDebugBorder(m_showDebugBorders);
            m_layerForHorizontalScrollbar->setName("horizontal scrollbar container");
#if PLATFORM(COCOA) && USE(CA)
            m_layerForHorizontalScrollbar->setAcceleratesDrawing(acceleratedDrawingEnabled());
#endif
            m_overflowControlsHostLayer->addChild(*m_layerForHorizontalScrollbar);

            if (auto* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), HorizontalScrollbar);
        }
    } else if (m_layerForHorizontalScrollbar) {
        GraphicsLayer::unparentAndClear(m_layerForHorizontalScrollbar);

        if (auto* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), HorizontalScrollbar);
    }

    if (requiresVerticalScrollbarLayer()) {
        if (!m_layerForVerticalScrollbar) {
            m_layerForVerticalScrollbar = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_layerForVerticalScrollbar->setCanDetachBackingStore(false);
            m_layerForVerticalScrollbar->setShowDebugBorder(m_showDebugBorders);
            m_layerForVerticalScrollbar->setName("vertical scrollbar container");
#if PLATFORM(COCOA) && USE(CA)
            m_layerForVerticalScrollbar->setAcceleratesDrawing(acceleratedDrawingEnabled());
#endif
            m_overflowControlsHostLayer->addChild(*m_layerForVerticalScrollbar);

            if (auto* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), VerticalScrollbar);
        }
    } else if (m_layerForVerticalScrollbar) {
        GraphicsLayer::unparentAndClear(m_layerForVerticalScrollbar);

        if (auto* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), VerticalScrollbar);
    }

    if (requiresScrollCornerLayer()) {
        if (!m_layerForScrollCorner) {
            m_layerForScrollCorner = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_layerForScrollCorner->setCanDetachBackingStore(false);
            m_layerForScrollCorner->setShowDebugBorder(m_showDebugBorders);
            m_layerForScrollCorner->setName("scroll corner");
#if PLATFORM(COCOA) && USE(CA)
            m_layerForScrollCorner->setAcceleratesDrawing(acceleratedDrawingEnabled());
#endif
            m_overflowControlsHostLayer->addChild(*m_layerForScrollCorner);
        }
    } else
        GraphicsLayer::unparentAndClear(m_layerForScrollCorner);

    m_renderView.frameView().positionScrollbarLayers();
}

void RenderLayerCompositor::ensureRootLayer()
{
    RootLayerAttachment expectedAttachment = isMainFrameCompositor() ? RootLayerAttachedViaChromeClient : RootLayerAttachedViaEnclosingFrame;
    if (expectedAttachment == m_rootLayerAttachment)
         return;

    if (!m_rootContentLayer) {
        m_rootContentLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
        m_rootContentLayer->setName("content root");
        IntRect overflowRect = snappedIntRect(m_renderView.layoutOverflowRect());
        m_rootContentLayer->setSize(FloatSize(overflowRect.maxX(), overflowRect.maxY()));
        m_rootContentLayer->setPosition(FloatPoint());

#if PLATFORM(IOS_FAMILY)
        // Page scale is applied above this on iOS, so we'll just say that our root layer applies it.
        auto& frame = m_renderView.frameView().frame();
        if (frame.isMainFrame())
            m_rootContentLayer->setAppliesPageScale();
#endif

        // Need to clip to prevent transformed content showing outside this frame
        updateRootContentLayerClipping();
    }

    if (requiresScrollLayer(expectedAttachment)) {
        if (!m_overflowControlsHostLayer) {
            ASSERT(!m_scrollLayer);
            ASSERT(!m_clipLayer);

            // Create a layer to host the clipping layer and the overflow controls layers.
            m_overflowControlsHostLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_overflowControlsHostLayer->setName("overflow controls host");

            // Create a clipping layer if this is an iframe
            m_clipLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_clipLayer->setName("frame clipping");
            m_clipLayer->setMasksToBounds(true);
            
            m_scrollLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_scrollLayer->setName("frame scrolling");

            // Hook them up
            m_overflowControlsHostLayer->addChild(*m_clipLayer);
            m_clipLayer->addChild(*m_scrollLayer);
            m_scrollLayer->addChild(*m_rootContentLayer);

            m_clipLayer->setSize(m_renderView.frameView().sizeForVisibleContent());
            m_clipLayer->setPosition(positionForClipLayer());
            m_clipLayer->setAnchorPoint(FloatPoint3D());

            updateOverflowControlsLayers();

            if (hasCoordinatedScrolling())
                scheduleLayerFlush(true);
            else
                updateScrollLayerPosition();
        }
    } else {
        if (m_overflowControlsHostLayer) {
            GraphicsLayer::unparentAndClear(m_overflowControlsHostLayer);
            GraphicsLayer::unparentAndClear(m_clipLayer);
            GraphicsLayer::unparentAndClear(m_scrollLayer);
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
    GraphicsLayer::unparentAndClear(m_layerForOverhangAreas);
#endif

    if (m_layerForHorizontalScrollbar) {
        GraphicsLayer::unparentAndClear(m_layerForHorizontalScrollbar);
        if (auto* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), HorizontalScrollbar);
        if (auto* horizontalScrollbar = m_renderView.frameView().verticalScrollbar())
            m_renderView.frameView().invalidateScrollbar(*horizontalScrollbar, IntRect(IntPoint(0, 0), horizontalScrollbar->frameRect().size()));
    }

    if (m_layerForVerticalScrollbar) {
        GraphicsLayer::unparentAndClear(m_layerForVerticalScrollbar);
        if (auto* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), VerticalScrollbar);
        if (auto* verticalScrollbar = m_renderView.frameView().verticalScrollbar())
            m_renderView.frameView().invalidateScrollbar(*verticalScrollbar, IntRect(IntPoint(0, 0), verticalScrollbar->frameRect().size()));
    }

    if (m_layerForScrollCorner) {
        GraphicsLayer::unparentAndClear(m_layerForScrollCorner);
        m_renderView.frameView().invalidateScrollCorner(m_renderView.frameView().scrollCornerRect());
    }

    if (m_overflowControlsHostLayer) {
        GraphicsLayer::unparentAndClear(m_overflowControlsHostLayer);
        GraphicsLayer::unparentAndClear(m_clipLayer);
        GraphicsLayer::unparentAndClear(m_scrollLayer);
    }
    ASSERT(!m_scrollLayer);
    GraphicsLayer::unparentAndClear(m_rootContentLayer);

    m_layerUpdater = nullptr;
}

void RenderLayerCompositor::attachRootLayer(RootLayerAttachment attachment)
{
    if (!m_rootContentLayer)
        return;

    LOG(Compositing, "RenderLayerCompositor %p attachRootLayer %d", this, attachment);

    switch (attachment) {
        case RootLayerUnattached:
            ASSERT_NOT_REACHED();
            break;
        case RootLayerAttachedViaChromeClient: {
            auto& frame = m_renderView.frameView().frame();
            page().chrome().client().attachRootGraphicsLayer(frame, rootGraphicsLayer());
            if (frame.isMainFrame())
                page().chrome().client().attachViewOverlayGraphicsLayer(frame, &page().pageOverlayController().layerWithViewOverlays());
            break;
        }
        case RootLayerAttachedViaEnclosingFrame: {
            // The layer will get hooked up via RenderLayerBacking::updateConfiguration()
            // for the frame's renderer in the parent document.
            if (auto* ownerElement = m_renderView.document().ownerElement())
                ownerElement->scheduleInvalidateStyleAndLayerComposition();
            break;
        }
    }

    m_rootLayerAttachment = attachment;
    rootLayerAttachmentChanged();
    
    if (m_shouldFlushOnReattach) {
        scheduleLayerFlushNow();
        m_shouldFlushOnReattach = false;
    }
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

        if (auto* ownerElement = m_renderView.document().ownerElement())
            ownerElement->scheduleInvalidateStyleAndLayerComposition();
        break;
    }
    case RootLayerAttachedViaChromeClient: {
        auto& frame = m_renderView.frameView().frame();
        page().chrome().client().attachRootGraphicsLayer(frame, nullptr);
        if (frame.isMainFrame()) {
            page().chrome().client().attachViewOverlayGraphicsLayer(frame, nullptr);
            page().pageOverlayController().willDetachRootLayer();
        }
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
    // The document-relative page overlay layer (which is pinned to the main frame's layer tree)
    // is moved between different RenderLayerCompositors' layer trees, and needs to be
    // reattached whenever we swap in a new RenderLayerCompositor.
    if (m_rootLayerAttachment == RootLayerUnattached)
        return;

    auto& frame = m_renderView.frameView().frame();

    // The attachment can affect whether the RenderView layer's paintsIntoWindow() behavior,
    // so call updateDrawsContent() to update that.
    auto* layer = m_renderView.layer();
    if (auto* backing = layer ? layer->backing() : nullptr)
        backing->updateDrawsContent();

    if (!frame.isMainFrame())
        return;

    Ref<GraphicsLayer> overlayHost = page().pageOverlayController().layerWithDocumentOverlays();
    m_rootContentLayer->addChild(WTFMove(overlayHost));
}

void RenderLayerCompositor::notifyIFramesOfCompositingChange()
{
    // Compositing affects the answer to RenderIFrame::requiresAcceleratedCompositing(), so
    // we need to schedule a style recalc in our parent document.
    if (auto* ownerElement = m_renderView.document().ownerElement())
        ownerElement->scheduleInvalidateStyleAndLayerComposition();
}

bool RenderLayerCompositor::layerHas3DContent(const RenderLayer& layer) const
{
    const RenderStyle& style = layer.renderer().style();

    if (style.transformStyle3D() == TransformStyle3D::Preserve3D || style.hasPerspective() || style.transform().has3DOperation())
        return true;

    const_cast<RenderLayer&>(layer).updateLayerListsIfNeeded();

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer&>(layer));
#endif

    for (auto* renderLayer : layer.negativeZOrderLayers()) {
        if (layerHas3DContent(*renderLayer))
            return true;
    }

    for (auto* renderLayer : layer.positiveZOrderLayers()) {
        if (layerHas3DContent(*renderLayer))
            return true;
    }

    for (auto* renderLayer : layer.normalFlowLayers()) {
        if (layerHas3DContent(*renderLayer))
            return true;
    }

    return false;
}

void RenderLayerCompositor::deviceOrPageScaleFactorChanged()
{
    // Page scale will only be applied at to the RenderView and sublayers, but the device scale factor
    // needs to be applied at the level of rootGraphicsLayer().
    if (auto* rootLayer = rootGraphicsLayer())
        rootLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
}

static bool canCoordinateScrollingForLayer(const RenderLayer& layer)
{
    return (layer.isRenderViewLayer() || layer.parent()) && layer.isComposited();
}

void RenderLayerCompositor::updateScrollCoordinatedStatus(RenderLayer& layer, OptionSet<ScrollingNodeChangeFlags> changes)
{
    OptionSet<LayerScrollCoordinationRole> coordinationRoles;
    if (isViewportConstrainedFixedOrStickyLayer(layer))
        coordinationRoles.add(ViewportConstrained);

    if (useCoordinatedScrollingForLayer(layer))
        coordinationRoles.add(Scrolling);

    if (layer.isComposited())
        layer.backing()->setIsScrollCoordinatedWithViewportConstrainedRole(coordinationRoles.contains(ViewportConstrained));

    if (layer.backing() && layer.backing()->updateConfiguration())
        setCompositingLayersNeedRebuild();

    if (coordinationRoles && canCoordinateScrollingForLayer(layer)) {
        if (m_scrollCoordinatedLayers.add(&layer).isNewEntry)
            m_subframeScrollLayersNeedReattach = true;

        updateScrollCoordinatedLayer(layer, coordinationRoles, changes);
    } else
        removeFromScrollCoordinatedLayers(layer);
}

void RenderLayerCompositor::removeFromScrollCoordinatedLayers(RenderLayer& layer)
{
    if (!m_scrollCoordinatedLayers.contains(&layer))
        return;

    m_subframeScrollLayersNeedReattach = true;
    
    m_scrollCoordinatedLayers.remove(&layer);
    m_scrollCoordinatedLayersNeedingUpdate.remove(&layer);

    detachScrollCoordinatedLayer(layer, { Scrolling, ViewportConstrained });
}

FixedPositionViewportConstraints RenderLayerCompositor::computeFixedViewportConstraints(RenderLayer& layer) const
{
    ASSERT(layer.isComposited());

    auto* graphicsLayer = layer.backing()->graphicsLayer();

    FixedPositionViewportConstraints constraints;
    constraints.setLayerPositionAtLastLayout(graphicsLayer->position());
    constraints.setViewportRectAtLastLayout(m_renderView.frameView().rectForFixedPositionLayout());
    constraints.setAlignmentOffset(graphicsLayer->pixelAlignmentOffset());

    const RenderStyle& style = layer.renderer().style();
    if (!style.left().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeLeft);

    if (!style.right().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeRight);

    if (!style.top().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeTop);

    if (!style.bottom().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeBottom);

    // If left and right are auto, use left.
    if (style.left().isAuto() && style.right().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeLeft);

    // If top and bottom are auto, use top.
    if (style.top().isAuto() && style.bottom().isAuto())
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeTop);
        
    return constraints;
}

StickyPositionViewportConstraints RenderLayerCompositor::computeStickyViewportConstraints(RenderLayer& layer) const
{
    ASSERT(layer.isComposited());
#if !PLATFORM(IOS_FAMILY)
    // We should never get here for stickies constrained by an enclosing clipping layer.
    // FIXME: Why does this assertion fail on iOS?
    ASSERT(!layer.enclosingOverflowClipLayer(ExcludeSelf));
#endif

    auto& renderer = downcast<RenderBoxModelObject>(layer.renderer());

    StickyPositionViewportConstraints constraints;
    renderer.computeStickyPositionConstraints(constraints, renderer.constrainingRectForStickyPosition());

    auto* graphicsLayer = layer.backing()->graphicsLayer();
    constraints.setLayerPositionAtLastLayout(graphicsLayer->position());
    constraints.setStickyOffsetAtLastLayout(renderer.stickyPositionOffset());
    constraints.setAlignmentOffset(graphicsLayer->pixelAlignmentOffset());

    return constraints;
}

static ScrollingNodeID enclosingScrollingNodeID(RenderLayer& layer, IncludeSelfOrNot includeSelf)
{
    auto* currLayer = includeSelf == IncludeSelf ? &layer : layer.parent();
    while (currLayer) {
        if (auto* backing = currLayer->backing()) {
            if (ScrollingNodeID nodeID = backing->scrollingNodeIDForChildren())
                return nodeID;
        }
        currLayer = currLayer->parent();
    }

    return 0;
}

static ScrollingNodeID scrollCoordinatedAncestorInParentOfFrame(Frame& frame)
{
    if (!frame.document() || !frame.view())
        return 0;

    // Find the frame's enclosing layer in our render tree.
    auto* ownerElement = frame.document()->ownerElement();
    auto* frameRenderer = ownerElement ? ownerElement->renderer() : nullptr;
    if (!frameRenderer)
        return 0;

    auto* layerInParentDocument = frameRenderer->enclosingLayer();
    if (!layerInParentDocument)
        return 0;

    return enclosingScrollingNodeID(*layerInParentDocument, IncludeSelf);
}

void RenderLayerCompositor::reattachSubframeScrollLayers()
{
    if (!m_subframeScrollLayersNeedReattach)
        return;
    
    m_subframeScrollLayersNeedReattach = false;

    auto* scrollingCoordinator = this->scrollingCoordinator();

    for (Frame* child = m_renderView.frameView().frame().tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!child->document() || !child->view())
            continue;

        // Ignore frames that are not scroll-coordinated.
        auto* childFrameView = child->view();
        ScrollingNodeID frameScrollingNodeID = childFrameView->scrollLayerID();
        if (!frameScrollingNodeID)
            continue;

        ScrollingNodeID parentNodeID = scrollCoordinatedAncestorInParentOfFrame(*child);
        if (!parentNodeID)
            continue;

        scrollingCoordinator->attachToStateTree(child->isMainFrame() ? MainFrameScrollingNode : SubframeScrollingNode, frameScrollingNodeID, parentNodeID);
    }
}

static inline LayerScrollCoordinationRole scrollCoordinationRoleForNodeType(ScrollingNodeType nodeType)
{
    switch (nodeType) {
    case MainFrameScrollingNode:
    case SubframeScrollingNode:
    case OverflowScrollingNode:
        return Scrolling;
    case FixedNode:
    case StickyNode:
        return ViewportConstrained;
    }
    ASSERT_NOT_REACHED();
    return Scrolling;
}

ScrollingNodeID RenderLayerCompositor::attachScrollingNode(RenderLayer& layer, ScrollingNodeType nodeType, ScrollingNodeID parentNodeID)
{
    auto* scrollingCoordinator = this->scrollingCoordinator();
    auto* backing = layer.backing();
    // Crash logs suggest that backing can be null here, but we don't know how: rdar://problem/18545452.
    ASSERT(backing);
    if (!backing)
        return 0;

    LayerScrollCoordinationRole role = scrollCoordinationRoleForNodeType(nodeType);
    ScrollingNodeID nodeID = backing->scrollingNodeIDForRole(role);
    if (!nodeID)
        nodeID = scrollingCoordinator->uniqueScrollLayerID();

    nodeID = scrollingCoordinator->attachToStateTree(nodeType, nodeID, parentNodeID);
    if (!nodeID)
        return 0;

    backing->setScrollingNodeIDForRole(nodeID, role);
    m_scrollingNodeToLayerMap.add(nodeID, &layer);
    
    return nodeID;
}

void RenderLayerCompositor::detachScrollCoordinatedLayer(RenderLayer& layer, OptionSet<LayerScrollCoordinationRole> roles)
{
    auto* backing = layer.backing();
    if (!backing)
        return;

    if (roles & Scrolling) {
        if (ScrollingNodeID nodeID = backing->scrollingNodeIDForRole(Scrolling))
            m_scrollingNodeToLayerMap.remove(nodeID);
    }

    if (roles & ViewportConstrained) {
        if (ScrollingNodeID nodeID = backing->scrollingNodeIDForRole(ViewportConstrained))
            m_scrollingNodeToLayerMap.remove(nodeID);
    }

    backing->detachFromScrollingCoordinator(roles);
}

void RenderLayerCompositor::updateScrollCoordinationForThisFrame(ScrollingNodeID parentNodeID)
{
    auto* scrollingCoordinator = this->scrollingCoordinator();
    ASSERT(scrollingCoordinator->coordinatesScrollingForFrameView(m_renderView.frameView()));

    ScrollingNodeID nodeID = attachScrollingNode(*m_renderView.layer(), m_renderView.frame().isMainFrame() ? MainFrameScrollingNode : SubframeScrollingNode, parentNodeID);
    scrollingCoordinator->updateFrameScrollingNode(nodeID, m_scrollLayer.get(), m_rootContentLayer.get(), fixedRootBackgroundLayer(), clipLayer());
}

void RenderLayerCompositor::updateScrollCoordinatedLayer(RenderLayer& layer, OptionSet<LayerScrollCoordinationRole> reasons, OptionSet<ScrollingNodeChangeFlags> changes)
{
    bool isRenderViewLayer = layer.isRenderViewLayer();

    ASSERT(m_scrollCoordinatedLayers.contains(&layer));
    ASSERT(layer.isComposited());

    auto* scrollingCoordinator = this->scrollingCoordinator();
    if (!scrollingCoordinator || !scrollingCoordinator->coordinatesScrollingForFrameView(m_renderView.frameView()))
        return;

    if (!m_renderView.frame().isMainFrame()) {
        ScrollingNodeID parentDocumentHostingNodeID = scrollCoordinatedAncestorInParentOfFrame(m_renderView.frame());
        if (!parentDocumentHostingNodeID)
            return;

        updateScrollCoordinationForThisFrame(parentDocumentHostingNodeID);
        if (!(reasons & ViewportConstrained) && isRenderViewLayer)
            return;
    }

    ScrollingNodeID parentNodeID = enclosingScrollingNodeID(layer, ExcludeSelf);
    if (!parentNodeID && !isRenderViewLayer)
        return;

    auto* backing = layer.backing();

    // Always call this even if the backing is already attached because the parent may have changed.
    // If a node plays both roles, fixed/sticky is always the ancestor node of scrolling.
    if (reasons & ViewportConstrained) {
        ScrollingNodeType nodeType = MainFrameScrollingNode;
        if (layer.renderer().isFixedPositioned())
            nodeType = FixedNode;
        else if (layer.renderer().style().position() == PositionType::Sticky)
            nodeType = StickyNode;
        else
            ASSERT_NOT_REACHED();

        ScrollingNodeID nodeID = attachScrollingNode(layer, nodeType, parentNodeID);
        if (!nodeID)
            return;
            
        LOG_WITH_STREAM(Compositing, stream << "Registering ViewportConstrained " << nodeType << " node " << nodeID << " (layer " << backing->graphicsLayer()->primaryLayerID() << ") as child of " << parentNodeID);

        if (changes & ScrollingNodeChangeFlags::Layer)
            scrollingCoordinator->updateNodeLayer(nodeID, backing->graphicsLayer());

        if (changes & ScrollingNodeChangeFlags::LayerGeometry) {
            switch (nodeType) {
            case FixedNode:
                scrollingCoordinator->updateNodeViewportConstraints(nodeID, computeFixedViewportConstraints(layer));
                break;
            case StickyNode:
                scrollingCoordinator->updateNodeViewportConstraints(nodeID, computeStickyViewportConstraints(layer));
                break;
            case MainFrameScrollingNode:
            case SubframeScrollingNode:
            case OverflowScrollingNode:
                break;
            }
        }
        
        parentNodeID = nodeID;
    } else
        detachScrollCoordinatedLayer(layer, ViewportConstrained);
        
    if (reasons & Scrolling) {
        if (isRenderViewLayer)
            updateScrollCoordinationForThisFrame(parentNodeID);
        else {
            ScrollingNodeType nodeType = OverflowScrollingNode;
            ScrollingNodeID nodeID = attachScrollingNode(layer, nodeType, parentNodeID);
            if (!nodeID)
                return;

            ScrollingCoordinator::ScrollingGeometry scrollingGeometry;
            scrollingGeometry.scrollOrigin = layer.scrollOrigin();
            scrollingGeometry.scrollPosition = layer.scrollPosition();
            scrollingGeometry.scrollableAreaSize = layer.visibleSize();
            scrollingGeometry.contentSize = layer.contentsSize();
            scrollingGeometry.reachableContentSize = layer.scrollableContentsSize();
#if ENABLE(CSS_SCROLL_SNAP)
            if (const Vector<LayoutUnit>* offsets = layer.horizontalSnapOffsets())
                scrollingGeometry.horizontalSnapOffsets = *offsets;
            if (const Vector<LayoutUnit>* offsets = layer.verticalSnapOffsets())
                scrollingGeometry.verticalSnapOffsets = *offsets;
            if (const Vector<ScrollOffsetRange<LayoutUnit>>* ranges = layer.horizontalSnapOffsetRanges())
                scrollingGeometry.horizontalSnapOffsetRanges = *ranges;
            if (const Vector<ScrollOffsetRange<LayoutUnit>>* ranges = layer.verticalSnapOffsetRanges())
                scrollingGeometry.verticalSnapOffsetRanges = *ranges;
            scrollingGeometry.currentHorizontalSnapPointIndex = layer.currentHorizontalSnapPointIndex();
            scrollingGeometry.currentVerticalSnapPointIndex = layer.currentVerticalSnapPointIndex();
#endif

            LOG(Compositing, "Registering Scrolling scrolling node %" PRIu64 " (layer %" PRIu64 ") as child of %" PRIu64, nodeID, backing->graphicsLayer()->primaryLayerID(), parentNodeID);

            scrollingCoordinator->updateOverflowScrollingNode(nodeID, backing->scrollingLayer(), backing->scrollingContentsLayer(), &scrollingGeometry);
        }
    } else
        detachScrollCoordinatedLayer(layer, Scrolling);
}

ScrollableArea* RenderLayerCompositor::scrollableAreaForScrollLayerID(ScrollingNodeID nodeID) const
{
    if (!nodeID)
        return nullptr;

    return m_scrollingNodeToLayerMap.get(nodeID);
}

#if PLATFORM(IOS_FAMILY)
typedef HashMap<PlatformLayer*, std::unique_ptr<ViewportConstraints>> LayerMap;
typedef HashMap<PlatformLayer*, PlatformLayer*> StickyContainerMap;

void RenderLayerCompositor::registerAllViewportConstrainedLayers()
{
    // Only the main frame should register fixed/sticky layers.
    if (!isMainFrameCompositor())
        return;

    if (scrollingCoordinator())
        return;

    LayerMap layerMap;
    StickyContainerMap stickyContainerMap;

    for (auto* layer : m_scrollCoordinatedLayers) {
        ASSERT(layer->isComposited());

        std::unique_ptr<ViewportConstraints> constraints;
        if (layer->renderer().isStickilyPositioned()) {
            constraints = std::make_unique<StickyPositionViewportConstraints>(computeStickyViewportConstraints(*layer));
            const RenderLayer* enclosingTouchScrollableLayer = nullptr;
            if (isAsyncScrollableStickyLayer(*layer, &enclosingTouchScrollableLayer) && enclosingTouchScrollableLayer) {
                ASSERT(enclosingTouchScrollableLayer->isComposited());
                stickyContainerMap.add(layer->backing()->graphicsLayer()->platformLayer(), enclosingTouchScrollableLayer->backing()->scrollingLayer()->platformLayer());
            }
        } else if (layer->renderer().isFixedPositioned())
            constraints = std::make_unique<FixedPositionViewportConstraints>(computeFixedViewportConstraints(*layer));
        else
            continue;

        layerMap.add(layer->backing()->graphicsLayer()->platformLayer(), WTFMove(constraints));
    }
    
    page().chrome().client().updateViewportConstrainedLayers(layerMap, stickyContainerMap);
}

void RenderLayerCompositor::unregisterAllViewportConstrainedLayers()
{
    // Only the main frame should register fixed/sticky layers.
    if (!isMainFrameCompositor())
        return;

    if (scrollingCoordinator())
        return;

    LayerMap layerMap;
    StickyContainerMap stickyContainerMap;
    page().chrome().client().updateViewportConstrainedLayers(layerMap, stickyContainerMap);
}

void RenderLayerCompositor::registerAllScrollingLayers()
{
    for (auto* layer : m_scrollingLayers)
        updateScrollingLayerWithClient(*layer, page().chrome().client());
}

void RenderLayerCompositor::unregisterAllScrollingLayers()
{
    for (auto* layer : m_scrollingLayers) {
        auto* backing = layer->backing();
        ASSERT(backing);
        page().chrome().client().removeScrollingLayer(layer->renderer().element(), backing->scrollingLayer()->platformLayer(), backing->scrollingContentsLayer()->platformLayer());
    }
}
#endif

void RenderLayerCompositor::willRemoveScrollingLayerWithBacking(RenderLayer& layer, RenderLayerBacking& backing)
{
    if (auto* scrollingCoordinator = this->scrollingCoordinator()) {
        backing.detachFromScrollingCoordinator(Scrolling);

        // For Coordinated Graphics.
        scrollingCoordinator->scrollableAreaScrollLayerDidChange(layer);
        return;
    }

#if PLATFORM(IOS_FAMILY)
    m_scrollingLayersNeedingUpdate.remove(&layer);
    m_scrollingLayers.remove(&layer);

    if (m_renderView.document().pageCacheState() != Document::NotInPageCache)
        return;

    auto* scrollingLayer = backing.scrollingLayer()->platformLayer();
    auto* contentsLayer = backing.scrollingContentsLayer()->platformLayer();
    page().chrome().client().removeScrollingLayer(layer.renderer().element(), scrollingLayer, contentsLayer);
#endif
}

void RenderLayerCompositor::didAddScrollingLayer(RenderLayer& layer)
{
    updateScrollCoordinatedStatus(layer, { ScrollingNodeChangeFlags::Layer, ScrollingNodeChangeFlags::LayerGeometry });

    if (auto* scrollingCoordinator = this->scrollingCoordinator()) {
        // For Coordinated Graphics.
        scrollingCoordinator->scrollableAreaScrollLayerDidChange(layer);
        return;
    }

#if PLATFORM(IOS_FAMILY)
    ASSERT(m_renderView.document().pageCacheState() == Document::NotInPageCache);
    m_scrollingLayers.add(&layer);
#endif
}

void RenderLayerCompositor::windowScreenDidChange(PlatformDisplayID displayID)
{
    if (m_layerUpdater)
        m_layerUpdater->screenDidChange(displayID);
}

ScrollingCoordinator* RenderLayerCompositor::scrollingCoordinator() const
{
    return page().scrollingCoordinator();
}

GraphicsLayerFactory* RenderLayerCompositor::graphicsLayerFactory() const
{
    return page().chrome().client().graphicsLayerFactory();
}

void RenderLayerCompositor::setLayerFlushThrottlingEnabled(bool enabled)
{
    m_layerFlushThrottlingEnabled = enabled;
    if (m_layerFlushThrottlingEnabled)
        return;
    m_layerFlushTimer.stop();
    if (!m_hasPendingLayerFlush)
        return;
    scheduleLayerFlushNow();
}

void RenderLayerCompositor::disableLayerFlushThrottlingTemporarilyForInteraction()
{
    if (m_layerFlushThrottlingTemporarilyDisabledForInteraction)
        return;
    m_layerFlushThrottlingTemporarilyDisabledForInteraction = true;
}

bool RenderLayerCompositor::isThrottlingLayerFlushes() const
{
    if (!m_layerFlushThrottlingEnabled)
        return false;
    if (!m_layerFlushTimer.isActive())
        return false;
    if (m_layerFlushThrottlingTemporarilyDisabledForInteraction)
        return false;
    return true;
}

void RenderLayerCompositor::startLayerFlushTimerIfNeeded()
{
    m_layerFlushThrottlingTemporarilyDisabledForInteraction = false;
    m_layerFlushTimer.stop();
    if (!m_layerFlushThrottlingEnabled)
        return;
    m_layerFlushTimer.startOneShot(throttledLayerFlushDelay);
}

void RenderLayerCompositor::startInitialLayerFlushTimerIfNeeded()
{
    if (!m_layerFlushThrottlingEnabled)
        return;
    if (m_layerFlushTimer.isActive())
        return;
    m_layerFlushTimer.startOneShot(throttledLayerFlushInitialDelay);
}

void RenderLayerCompositor::layerFlushTimerFired()
{
    if (!m_hasPendingLayerFlush)
        return;
    scheduleLayerFlushNow();
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<DisplayRefreshMonitor> RenderLayerCompositor::createDisplayRefreshMonitor(PlatformDisplayID displayID) const
{
    if (auto monitor = page().chrome().client().createDisplayRefreshMonitor(displayID))
        return monitor;

    return DisplayRefreshMonitor::createDefaultDisplayRefreshMonitor(displayID);
}
#endif

#if ENABLE(CSS_SCROLL_SNAP)
void RenderLayerCompositor::updateScrollSnapPropertiesWithFrameView(const FrameView& frameView)
{
    if (auto* coordinator = scrollingCoordinator())
        coordinator->updateScrollSnapPropertiesWithFrameView(frameView);
}
#endif

Page& RenderLayerCompositor::page() const
{
    return m_renderView.page();
}

TextStream& operator<<(TextStream& ts, CompositingUpdateType updateType)
{
    switch (updateType) {
    case CompositingUpdateType::AfterStyleChange: ts << "after style change"; break;
    case CompositingUpdateType::AfterLayout: ts << "after layout"; break;
    case CompositingUpdateType::OnHitTest: ts << "on hit test"; break;
    case CompositingUpdateType::OnScroll: ts << "on scroll"; break;
    case CompositingUpdateType::OnCompositedScroll: ts << "on composited scroll"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CompositingPolicy compositingPolicy)
{
    switch (compositingPolicy) {
    case CompositingPolicy::Normal: ts << "normal"; break;
    case CompositingPolicy::Conservative: ts << "conservative"; break;
    }
    return ts;
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
void showGraphicsLayerTreeForCompositor(WebCore::RenderLayerCompositor& compositor)
{
    showGraphicsLayerTree(compositor.rootGraphicsLayer());
}
#endif
