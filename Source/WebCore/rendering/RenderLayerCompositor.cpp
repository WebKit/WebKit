/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
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

#include "AsyncScrollingCoordinator.h"
#include "CSSPropertyNames.h"
#include "CanvasRenderingContext.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Frame.h"
#include "FrameView.h"
#include "FullscreenManager.h"
#include "GraphicsLayer.h"
#include "HTMLCanvasElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "InspectorInstrumentation.h"
#include "KeyframeEffectStack.h"
#include "LayerAncestorClippingStack.h"
#include "LayerOverlapMap.h"
#include "Logging.h"
#include "NodeList.h"
#include "Page.h"
#include "PageOverlayController.h"
#include "RenderEmbeddedObject.h"
#include "RenderFragmentedFlow.h"
#include "RenderGeometryMap.h"
#include "RenderIFrame.h"
#include "RenderImage.h"
#include "RenderLayerBacking.h"
#include "RenderLayerScrollableArea.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "ScrollingConstraints.h"
#include "Settings.h"
#include "TiledBacking.h"
#include "TransformState.h"
#include <wtf/HexNumber.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>
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

#if ENABLE(MODEL_ELEMENT)
#include "RenderModel.h"
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

using namespace HTMLNames;

struct ScrollingTreeState {
    std::optional<ScrollingNodeID> parentNodeID;
    size_t nextChildIndex { 0 };
    bool needSynchronousScrollingReasonsUpdate { false };
};

struct RenderLayerCompositor::OverlapExtent {
    LayoutRect bounds;
    bool extentComputed { false };
    bool hasTransformAnimation { false };
    bool animationCausesExtentUncertainty { false };

    bool knownToBeHaveExtentUncertainty() const { return extentComputed && animationCausesExtentUncertainty; }
};

struct RenderLayerCompositor::CompositingState {
    CompositingState(RenderLayer* compAncestor, bool testOverlap = true)
        : compositingAncestor(compAncestor)
        , testingOverlap(testOverlap)
    {
    }
    
    CompositingState stateForPaintOrderChildren(RenderLayer& layer) const
    {
        UNUSED_PARAM(layer);
        CompositingState childState(compositingAncestor);
        if (layer.isStackingContext())
            childState.stackingContextAncestor = &layer;
        else
            childState.stackingContextAncestor = stackingContextAncestor;

        childState.backingSharingAncestor = backingSharingAncestor;
        childState.subtreeIsCompositing = false;
        childState.testingOverlap = testingOverlap;
        childState.fullPaintOrderTraversalRequired = fullPaintOrderTraversalRequired;
        childState.descendantsRequireCompositingUpdate = descendantsRequireCompositingUpdate;
        childState.ancestorHasTransformAnimation = ancestorHasTransformAnimation;
        childState.hasCompositedNonContainedDescendants = false;
#if ENABLE(CSS_COMPOSITING)
        childState.hasNotIsolatedCompositedBlendingDescendants = false; // FIXME: should this only be reset for stacking contexts?
#endif
#if !LOG_DISABLED
        childState.depth = depth + 1;
#endif
        return childState;
    }

    void updateWithDescendantStateAndLayer(const CompositingState& childState, const RenderLayer& layer, const RenderLayer* ancestorLayer, const OverlapExtent& layerExtent, bool isUnchangedSubtree = false)
    {
        // Subsequent layers in the parent stacking context also need to composite.
        subtreeIsCompositing |= childState.subtreeIsCompositing | layer.isComposited();
        if (!isUnchangedSubtree)
            fullPaintOrderTraversalRequired |= childState.fullPaintOrderTraversalRequired;

        // Turn overlap testing off for later layers if it's already off, or if we have an animating transform.
        // Note that if the layer clips its descendants, there's no reason to propagate the child animation to the parent layers. That's because
        // we know for sure the animation is contained inside the clipping rectangle, which is already added to the overlap map.
        auto canReenableOverlapTesting = [&layer] {
            return layer.isComposited() && RenderLayerCompositor::clipsCompositingDescendants(layer);
        };
        if ((!childState.testingOverlap && !canReenableOverlapTesting()) || layerExtent.knownToBeHaveExtentUncertainty())
            testingOverlap = false;

        auto computeHasCompositedNonContainedDescendants = [&] {
            if (hasCompositedNonContainedDescendants)
                return true;
            if (!ancestorLayer)
                return false;
            if (!layer.isComposited())
                return false;
            if (!layer.renderer().isOutOfFlowPositioned())
                return false;
            if (layer.ancestorLayerIsInContainingBlockChain(*ancestorLayer))
                return false;
            return true;
        };

        hasCompositedNonContainedDescendants = computeHasCompositedNonContainedDescendants();

#if ENABLE(CSS_COMPOSITING)
        if ((layer.isComposited() && layer.hasBlendMode()) || (layer.hasNotIsolatedCompositedBlendingDescendants() && !layer.isolatesCompositedBlending()))
            hasNotIsolatedCompositedBlendingDescendants = true;
#endif
    }

    bool hasNonRootCompositedAncestor() const
    {
        return compositingAncestor && !compositingAncestor->isRenderViewLayer();
    }

    RenderLayer* compositingAncestor;
    RenderLayer* backingSharingAncestor { nullptr };
    RenderLayer* stackingContextAncestor { nullptr };
    bool subtreeIsCompositing { false };
    bool testingOverlap { true };
    bool fullPaintOrderTraversalRequired { false };
    bool descendantsRequireCompositingUpdate { false };
    bool ancestorHasTransformAnimation { false };
    bool hasCompositedNonContainedDescendants { false };
#if ENABLE(CSS_COMPOSITING)
    bool hasNotIsolatedCompositedBlendingDescendants { false };
#endif
#if !LOG_DISABLED
    unsigned depth { 0 };
#endif
};

struct RenderLayerCompositor::UpdateBackingTraversalState {
    UpdateBackingTraversalState(RenderLayer* compAncestor = nullptr, Vector<RenderLayer*>* clippedLayers = nullptr, Vector<RenderLayer*>* overflowScrollers = nullptr)
        : compositingAncestor(compAncestor)
        , layersClippedByScrollers(clippedLayers)
        , overflowScrollLayers(overflowScrollers)
    {
    }

    UpdateBackingTraversalState stateForDescendants() const
    {
        UpdateBackingTraversalState state(compositingAncestor, layersClippedByScrollers, overflowScrollLayers);
#if !LOG_DISABLED
        state.depth = depth + 1;
#endif
        return state;
    }

    RenderLayer* compositingAncestor;

    // List of layers in the current stacking context that are clipped by ancestor scrollers.
    Vector<RenderLayer*>* layersClippedByScrollers;
    // List of layers with composited overflow:scroll.
    Vector<RenderLayer*>* overflowScrollLayers;

#if !LOG_DISABLED
    unsigned depth { 0 };
#endif
};

class RenderLayerCompositor::BackingSharingState {
    WTF_MAKE_NONCOPYABLE(BackingSharingState);
public:
    BackingSharingState() = default;

    RenderLayer* backingProviderCandidate() const { return m_backingProviderCandidate; };
    
    void appendSharingLayer(RenderLayer& layer)
    {
        ASSERT(m_backingProviderCandidate);
        m_backingSharingLayers.append(layer);
    }

    RenderLayer* updateBeforeDescendantTraversal(RenderLayer&, bool willBeComposited);
    void updateAfterDescendantTraversal(RenderLayer&, RenderLayer* preDescendantProviderCandidate, RenderLayer* stackingContextAncestor);
    
    bool isPotentialBackingSharingLayer(const RenderLayer& layer) const
    {
        return m_backingSharingLayers.contains(&layer);
    }
    
    bool canIncludeLayer(const RenderLayer&) const;

    // Add a layer that would repaint into a layer in m_backingSharingLayers.
    // That repaint has to wait until we've set the provider's backing-sharing layers.
    void addLayerNeedingRepaint(RenderLayer& layer)
    {
        m_layersPendingRepaint.add(layer);
    }

private:
    void layerWillBeComposited(RenderLayer&);

    void startBackingSharingSequence(RenderLayer& candidateLayer, RenderLayer* candidateStackingContext);
    void endBackingSharingSequence();
    void issuePendingRepaints();

    RenderLayer* m_backingProviderCandidate { nullptr };
    RenderLayer* m_backingProviderStackingContext { nullptr };
    Vector<WeakPtr<RenderLayer>> m_backingSharingLayers;
    WeakHashSet<RenderLayer> m_layersPendingRepaint;
};

void RenderLayerCompositor::BackingSharingState::startBackingSharingSequence(RenderLayer& candidateLayer, RenderLayer* candidateStackingContext)
{
    ASSERT(!m_backingProviderCandidate);
    ASSERT(m_backingSharingLayers.isEmpty());

    m_backingProviderCandidate = &candidateLayer;
    m_backingProviderStackingContext = candidateStackingContext;
}

void RenderLayerCompositor::BackingSharingState::endBackingSharingSequence()
{
    if (!m_backingProviderCandidate)
        return;

    m_backingProviderCandidate->backing()->setBackingSharingLayers(WTFMove(m_backingSharingLayers));
    m_backingSharingLayers.clear();
    issuePendingRepaints();

    m_backingProviderCandidate = nullptr;
}

bool RenderLayerCompositor::BackingSharingState::canIncludeLayer(const RenderLayer& layer) const
{
    // Disable sharing when painting shared layers doesn't work correctly.
    if (layer.hasReflection())
        return false;

    return m_backingProviderCandidate && layer.ancestorLayerIsInContainingBlockChain(*m_backingProviderCandidate);
}

RenderLayer* RenderLayerCompositor::BackingSharingState::updateBeforeDescendantTraversal(RenderLayer& layer, bool willBeComposited)
{
    layer.setBackingProviderLayer(nullptr);

    LOG_WITH_STREAM(Compositing, stream << "BackingSharingState::updateBeforeDescendantTraversal: layer " << &layer << " will be composited " << willBeComposited);

    // A layer that composites resets backing-sharing, since subsequent layers need to composite to overlap it.
    if (willBeComposited) {
        LOG_WITH_STREAM(Compositing, stream << " ending sharing sequence on " << m_backingProviderCandidate);
        m_backingSharingLayers.removeAll(&layer);
        endBackingSharingSequence();
    }
    
    return m_backingProviderCandidate;
}

void RenderLayerCompositor::BackingSharingState::updateAfterDescendantTraversal(RenderLayer& layer, RenderLayer* preDescendantProviderCandidate, RenderLayer* stackingContextAncestor)
{
    LOG_WITH_STREAM(Compositing, stream << "BackingSharingState::updateAfterDescendantTraversal for layer " << &layer << " is composited " << layer.isComposited());

    if (layer.isComposited()) {
        // If this layer is being composited, clean up sharing-related state.
        layer.disconnectFromBackingProviderLayer();
        m_backingSharingLayers.removeAll(&layer);
    }

    // Backing sharing is constrained to layers in the same stacking context.
    if (&layer == m_backingProviderStackingContext) {
        ASSERT(&layer != m_backingProviderCandidate);
        LOG_WITH_STREAM(Compositing, stream << "BackingSharingState::updateAfterDescendantTraversal: End of stacking context for backing provider " << m_backingProviderCandidate << ", ending sharing sequence with " << m_backingSharingLayers.size() << " sharing layers");
        endBackingSharingSequence();

        if (layer.isComposited())
            layer.backing()->clearBackingSharingLayers();

        return;
    }

    if (!layer.isComposited())
        return;

    if (!m_backingProviderCandidate) {
        LOG_WITH_STREAM(Compositing, stream << "BackingSharingState::updateAfterDescendantTraversal: starting potential sharing sequence for " << &layer);
        startBackingSharingSequence(layer, stackingContextAncestor);
    } else {
        ASSERT(&layer != m_backingProviderCandidate);
        layer.backing()->clearBackingSharingLayers();
        LOG_WITH_STREAM(Compositing, stream << "BackingSharingState::updateAfterDescendantTraversal: " << &layer << " is composited; maybe ending existing backing sequence with candidate " << m_backingProviderCandidate << " context " << m_backingProviderStackingContext << " with " << m_backingSharingLayers.size() << " sharing layers");
        
        if (m_backingProviderCandidate && preDescendantProviderCandidate == m_backingProviderCandidate) {
            m_backingSharingLayers.removeAll(&layer);
            endBackingSharingSequence();
        }
    }
}

void RenderLayerCompositor::BackingSharingState::issuePendingRepaints()
{
    for (auto& layer : m_layersPendingRepaint) {
        LOG_WITH_STREAM(Compositing, stream << "Issuing postponed repaint of layer " << &layer);
        layer.computeRepaintRectsIncludingDescendants();
        layer.compositor().repaintOnCompositingChange(layer);
    }
    
    m_layersPendingRepaint.clear();
}

#if !LOG_DISABLED || ENABLE(TREE_DEBUGGING)
static inline bool compositingLogEnabled()
{
    return LogCompositing.state == WTFLogChannelState::On;
}

static inline bool layersLogEnabled()
{
    return LogLayers.state == WTFLogChannelState::On;
}
#endif

RenderLayerCompositor::RenderLayerCompositor(RenderView& renderView)
    : m_renderView(renderView)
    , m_updateCompositingLayersTimer(*this, &RenderLayerCompositor::updateCompositingLayersTimerFired)
{
#if PLATFORM(IOS_FAMILY)
    if (m_renderView.frameView().platformWidget())
        m_legacyScrollingLayerCoordinator = makeUnique<LegacyWebKitScrollingLayerCoordinator>(page().chrome().client(), isMainFrameCompositor());
#endif
}

RenderLayerCompositor::~RenderLayerCompositor()
{
    // Take care that the owned GraphicsLayers are deleted first as their destructors may call back here.
    GraphicsLayer::unparentAndClear(m_rootContentsLayer);
    
    GraphicsLayer::unparentAndClear(m_clipLayer);
    GraphicsLayer::unparentAndClear(m_scrollContainerLayer);
    GraphicsLayer::unparentAndClear(m_scrolledContentsLayer);

    GraphicsLayer::unparentAndClear(m_overflowControlsHostLayer);

    GraphicsLayer::unparentAndClear(m_layerForHorizontalScrollbar);
    GraphicsLayer::unparentAndClear(m_layerForVerticalScrollbar);
    GraphicsLayer::unparentAndClear(m_layerForScrollCorner);

#if HAVE(RUBBER_BANDING)
    GraphicsLayer::unparentAndClear(m_layerForOverhangAreas);
    GraphicsLayer::unparentAndClear(m_contentShadowLayer);
    GraphicsLayer::unparentAndClear(m_layerForTopOverhangArea);
    GraphicsLayer::unparentAndClear(m_layerForBottomOverhangArea);
    GraphicsLayer::unparentAndClear(m_layerForHeader);
    GraphicsLayer::unparentAndClear(m_layerForFooter);
#endif

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
        
        
        m_renderView.layer()->setNeedsPostLayoutCompositingUpdate();
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
        if (auto* rootLayer = m_renderView.layer()) {
            rootLayer->setNeedsCompositingConfigurationUpdate();
            rootLayer->setDescendantsNeedUpdateBackingAndHierarchyTraversal();
        }
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
        rootRenderLayer().setDescendantsNeedCompositingRequirementsTraversal();
}

void RenderLayerCompositor::cacheAcceleratedCompositingFlagsAfterLayout()
{
    cacheAcceleratedCompositingFlags();

    if (isMainFrameCompositor())
        return;

    RequiresCompositingData queryData;
    bool forceCompositingMode = m_hasAcceleratedCompositing && m_renderView.settings().forceCompositingMode() && requiresCompositingForScrollableFrame(queryData);
    if (forceCompositingMode != m_forceCompositingMode) {
        m_forceCompositingMode = forceCompositingMode;
        rootRenderLayer().setDescendantsNeedCompositingRequirementsTraversal();
    }
}

bool RenderLayerCompositor::updateCompositingPolicy()
{
    if (!usesCompositing())
        return false;

    auto currentPolicy = m_compositingPolicy;
    if (page().compositingPolicyOverride()) {
        m_compositingPolicy = page().compositingPolicyOverride().value();
        return m_compositingPolicy != currentPolicy;
    }
    
    static auto cachedMemoryPolicy = WTF::MemoryUsagePolicy::Unrestricted;
    static MonotonicTime cachedMemoryPolicyTime;
    static constexpr auto memoryPolicyCachingDuration = 2_s;
    auto now = MonotonicTime::now();
    if (now - cachedMemoryPolicyTime > memoryPolicyCachingDuration) {
        cachedMemoryPolicy = MemoryPressureHandler::singleton().currentMemoryUsagePolicy();
        cachedMemoryPolicyTime = now;
    }

    m_compositingPolicy = cachedMemoryPolicy == WTF::MemoryUsagePolicy::Unrestricted ? CompositingPolicy::Normal : CompositingPolicy::Conservative;
    return m_compositingPolicy != currentPolicy;
}

bool RenderLayerCompositor::canRender3DTransforms() const
{
    return hasAcceleratedCompositing() && (m_compositingTriggers & ChromeClient::ThreeDTransformTrigger);
}

void RenderLayerCompositor::willRecalcStyle()
{
    cacheAcceleratedCompositingFlags();
}

bool RenderLayerCompositor::didRecalcStyleWithNoPendingLayout()
{
    return updateCompositingLayers(CompositingUpdateType::AfterStyleChange);
}

void RenderLayerCompositor::customPositionForVisibleRectComputation(const GraphicsLayer* graphicsLayer, FloatPoint& position) const
{
    if (graphicsLayer != m_scrolledContentsLayer.get())
        return;

    FloatPoint scrollPosition = -position;

    if (m_renderView.frameView().scrollBehaviorForFixedElements() == StickToDocumentBounds)
        scrollPosition = m_renderView.frameView().constrainScrollPositionForOverhang(roundedIntPoint(scrollPosition));

    position = -scrollPosition;
}

bool RenderLayerCompositor::shouldDumpPropertyForLayer(const GraphicsLayer* layer, const char* propertyName, OptionSet<LayerTreeAsTextOptions>) const
{
    if (!strcmp(propertyName, "anchorPoint"))
        return layer->anchorPoint() != FloatPoint3D(0.5f, 0.5f, 0);

    return true;
}

void RenderLayerCompositor::notifyFlushRequired(const GraphicsLayer*)
{
    scheduleRenderingUpdate();
}

void RenderLayerCompositor::scheduleRenderingUpdate()
{
    ASSERT(!m_flushingLayers);

    page().scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
}

FloatRect RenderLayerCompositor::visibleRectForLayerFlushing() const
{
    const FrameView& frameView = m_renderView.frameView();
#if PLATFORM(IOS_FAMILY)
    return frameView.exposedContentRect();
#else
    // Having a m_scrolledContentsLayer indicates that we're doing scrolling via GraphicsLayers.
    FloatRect visibleRect = m_scrolledContentsLayer ? FloatRect({ }, frameView.sizeForVisibleContent()) : frameView.visibleContentRect();

    if (auto exposedRect = frameView.viewExposedRect())
        visibleRect.intersect(*exposedRect);

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
        m_shouldFlushOnReattach = true;
        return;
    }

    ASSERT(!m_flushingLayers);
    {
        SetForScope flushingLayersScope(m_flushingLayers, true);

        if (auto* rootLayer = rootGraphicsLayer()) {
#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
            LayerTreeHitTestLocker layerLocker(scrollingCoordinator());
#endif
            FloatRect visibleRect = visibleRectForLayerFlushing();
            LOG_WITH_STREAM(Compositing,  stream << "\nRenderLayerCompositor " << this << " flushPendingLayerChanges (is root " << isFlushRoot << ") visible rect " << visibleRect);
            rootLayer->flushCompositingState(visibleRect);
        }
        
        ASSERT(m_flushingLayers);

#if ENABLE(TREE_DEBUGGING)
        if (layersLogEnabled()) {
            LOG(Layers, "RenderLayerCompositor::flushPendingLayerChanges");
            showGraphicsLayerTree(rootGraphicsLayer());
        }
#endif
    }

#if PLATFORM(IOS_FAMILY)
    updateScrollCoordinatedLayersAfterFlushIncludingSubframes();

    if (isFlushRoot)
        page().chrome().client().didFlushCompositingLayers();
#endif

    ++m_layerFlushCount;
}

#if PLATFORM(IOS_FAMILY)
void RenderLayerCompositor::updateScrollCoordinatedLayersAfterFlushIncludingSubframes()
{
    updateScrollCoordinatedLayersAfterFlush();

    auto& frame = m_renderView.frameView().frame();
    for (AbstractFrame* subframe = frame.tree().firstChild(); subframe; subframe = subframe->tree().traverseNext(&frame)) {
        auto* localFrame = dynamicDowncast<LocalFrame>(subframe);
        if (!localFrame)
            continue;
        auto* view = localFrame->contentRenderer();
        if (!view)
            continue;

        view->compositor().updateScrollCoordinatedLayersAfterFlush();
    }
}

void RenderLayerCompositor::updateScrollCoordinatedLayersAfterFlush()
{
    if (m_legacyScrollingLayerCoordinator) {
        m_legacyScrollingLayerCoordinator->registerAllViewportConstrainedLayers(*this);
        m_legacyScrollingLayerCoordinator->registerAllScrollingLayers();
    }
}
#endif

void RenderLayerCompositor::didChangePlatformLayerForLayer(RenderLayer& layer, const GraphicsLayer*)
{
    auto* scrollingCoordinator = this->scrollingCoordinator();
    if (!scrollingCoordinator)
        return;

    auto* backing = layer.backing();
    if (auto nodeID = backing->scrollingNodeIDForRole(ScrollCoordinationRole::Scrolling))
        updateScrollingNodeLayers(nodeID, layer, *scrollingCoordinator);

    if (auto* clippingStack = layer.backing()->ancestorClippingStack())
        clippingStack->updateScrollingNodeLayers(*scrollingCoordinator);

    if (auto nodeID = backing->scrollingNodeIDForRole(ScrollCoordinationRole::ViewportConstrained))
        scrollingCoordinator->setNodeLayers(nodeID, { backing->viewportAnchorLayer() });

    if (auto nodeID = backing->scrollingNodeIDForRole(ScrollCoordinationRole::FrameHosting))
        scrollingCoordinator->setNodeLayers(nodeID, { backing->graphicsLayer() });

    if (auto nodeID = backing->scrollingNodeIDForRole(ScrollCoordinationRole::Positioning))
        scrollingCoordinator->setNodeLayers(nodeID, { backing->graphicsLayer() });
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
        scheduleRenderingUpdate();
}

void RenderLayerCompositor::notifyFlushBeforeDisplayRefresh(const GraphicsLayer*)
{
    if (!m_layerUpdater) {
        PlatformDisplayID displayID = page().chrome().displayID();
        m_layerUpdater = makeUnique<GraphicsLayerUpdater>(*this, displayID);
    }
    
    m_layerUpdater->scheduleUpdate();
}

void RenderLayerCompositor::flushLayersSoon(GraphicsLayerUpdater&)
{
    scheduleRenderingUpdate();
}

DisplayRefreshMonitorFactory* RenderLayerCompositor::displayRefreshMonitorFactory()
{
    return page().chrome().client().displayRefreshMonitorFactory();
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

void RenderLayerCompositor::cancelCompositingLayerUpdate()
{
    m_updateCompositingLayersTimer.stop();
}

template<typename ApplyFunctionType>
void RenderLayerCompositor::applyToCompositedLayerIncludingDescendants(RenderLayer& layer, const ApplyFunctionType& function)
{
    if (layer.isComposited())
        function(layer);
    for (auto* childLayer = layer.firstChild(); childLayer; childLayer = childLayer->nextSibling())
        applyToCompositedLayerIncludingDescendants(*childLayer, function);
}

void RenderLayerCompositor::updateEventRegions()
{
#if ENABLE(ASYNC_SCROLLING)
    applyToCompositedLayerIncludingDescendants(*m_renderView.layer(), [](auto& layer) {
        layer.backing()->updateEventRegion();
    });
#endif
}

static std::optional<ScrollingNodeID> frameHostingNodeForFrame(Frame& frame)
{
    if (!frame.document() || !frame.view())
        return { };

    // Find the frame's enclosing layer in our render tree.
    auto* ownerElement = frame.document()->ownerElement();
    if (!ownerElement)
        return { };

    auto* frameRenderer = ownerElement->renderer();
    if (!frameRenderer || !is<RenderWidget>(frameRenderer))
        return { };

    auto& widgetRenderer = downcast<RenderWidget>(*frameRenderer);
    if (!widgetRenderer.hasLayer() || !widgetRenderer.layer()->isComposited()) {
        LOG(Scrolling, "frameHostingNodeForFrame: frame renderer has no layer or is not composited.");
        return { };
    }

    if (auto frameHostingNodeID = widgetRenderer.layer()->backing()->scrollingNodeIDForRole(ScrollCoordinationRole::FrameHosting))
        return frameHostingNodeID;

    return { };
}

// Returns true on a successful update.
bool RenderLayerCompositor::updateCompositingLayers(CompositingUpdateType updateType, RenderLayer* updateRoot)
{
    LOG_WITH_STREAM(Compositing, stream << "RenderLayerCompositor " << this << " [" << m_renderView.frameView() << "] updateCompositingLayers " << updateType << " contentLayersCount " << m_contentLayersCount);

    TraceScope tracingScope(CompositingUpdateStart, CompositingUpdateEnd);

#if ENABLE(TREE_DEBUGGING)
    if (compositingLogEnabled())
        showPaintOrderTree(m_renderView.layer());
#endif

    if (updateType == CompositingUpdateType::AfterStyleChange || updateType == CompositingUpdateType::AfterLayout)
        cacheAcceleratedCompositingFlagsAfterLayout(); // Some flags (e.g. forceCompositingMode) depend on layout.

    m_updateCompositingLayersTimer.stop();

    ASSERT(m_renderView.document().backForwardCacheState() == Document::NotInBackForwardCache
        || m_renderView.document().backForwardCacheState() == Document::AboutToEnterBackForwardCache);
    
    // Compositing layers will be updated in Document::setVisualUpdatesAllowed(bool) if suppressed here.
    if (!m_renderView.document().visualUpdatesAllowed())
        return false;

    // Avoid updating the layers with old values. Compositing layers will be updated after the layout is finished.
    // This happens when m_updateCompositingLayersTimer fires before layout is updated.
    if (m_renderView.needsLayout()) {
        LOG_WITH_STREAM(Compositing, stream << "RenderLayerCompositor " << this << " updateCompositingLayers " << updateType << " - m_renderView.needsLayout, bailing ");
        return false;
    }

    if (!m_compositing && (m_forceCompositingMode || (isMainFrameCompositor() && page().pageOverlayController().overlayCount())))
        enableCompositingMode(true);

    bool isPageScroll = !updateRoot || updateRoot == &rootRenderLayer();
    updateRoot = &rootRenderLayer();

    if (updateType == CompositingUpdateType::OnScroll || updateType == CompositingUpdateType::OnCompositedScroll) {
        // We only get here if we didn't scroll on the scrolling thread, so this update needs to re-position viewport-constrained layers.
        if (m_renderView.settings().acceleratedCompositingForFixedPositionEnabled() && isPageScroll) {
            if (auto* viewportConstrainedObjects = m_renderView.frameView().viewportConstrainedObjects()) {
                for (auto& renderer : *viewportConstrainedObjects) {
                    if (auto* layer = renderer.layer())
                        layer->setNeedsCompositingGeometryUpdate();
                }
            }
        }

        // Scrolling can affect overlap. FIXME: avoid for page scrolling.
        updateRoot->setDescendantsNeedCompositingRequirementsTraversal();
    }

    if (updateType == CompositingUpdateType::AfterLayout) {
        // Ensure that post-layout updates push new scroll position and viewport rects onto the root node.
        rootRenderLayer().setNeedsScrollingTreeUpdate();
    }

    if (!updateRoot->hasDescendantNeedingCompositingRequirementsTraversal() && !m_compositing) {
        LOG_WITH_STREAM(Compositing, stream << " no compositing work to do");
        return true;
    }

    if (!updateRoot->needsAnyCompositingTraversal()) {
        LOG_WITH_STREAM(Compositing, stream << " updateRoot has no dirty child and doesn't need update");
        return true;
    }

    ++m_compositingUpdateCount;

#if !LOG_DISABLED
    MonotonicTime startTime;
    if (compositingLogEnabled()) {
        ++m_rootLayerUpdateCount;
        startTime = MonotonicTime::now();
    }

    if (compositingLogEnabled()) {
        m_obligateCompositedLayerCount = 0;
        m_secondaryCompositedLayerCount = 0;
        m_obligatoryBackingStoreBytes = 0;
        m_secondaryBackingStoreBytes = 0;

        auto& frame = m_renderView.frameView().frame();
        bool isMainFrame = isMainFrameCompositor();
        LOG_WITH_STREAM(Compositing, stream << "\nUpdate " << m_rootLayerUpdateCount << " of " << (isMainFrame ? "main frame"_s : makeString("frame "_s, frame.frameID().object().toUInt64())) << " - compositing policy is " << m_compositingPolicy);
    }
#endif

    // FIXME: optimize root-only update.
    if (updateRoot->hasDescendantNeedingCompositingRequirementsTraversal() || updateRoot->needsCompositingRequirementsTraversal()) {
        auto& rootLayer = rootRenderLayer();
        CompositingState compositingState(updateRoot);
        BackingSharingState backingSharingState;
        LayerOverlapMap overlapMap(rootLayer);

        bool descendantHas3DTransform = false;
        computeCompositingRequirements(nullptr, rootLayer, overlapMap, compositingState, backingSharingState, descendantHas3DTransform);
    }

    LOG(Compositing, "\nRenderLayerCompositor::updateCompositingLayers - mid");
#if ENABLE(TREE_DEBUGGING)
    if (compositingLogEnabled())
        showPaintOrderTree(m_renderView.layer());
#endif

    if (updateRoot->hasDescendantNeedingUpdateBackingOrHierarchyTraversal() || updateRoot->needsUpdateBackingOrHierarchyTraversal()) {
        ScrollingTreeState scrollingTreeState = { 0, 0 };
        if (!m_renderView.frame().isMainFrame())
            scrollingTreeState.parentNodeID = frameHostingNodeForFrame(m_renderView.frame());

        auto* scrollingCoordinator = this->scrollingCoordinator();
        bool hadSubscrollers = scrollingCoordinator ? scrollingCoordinator->hasSubscrollers() : false;

        UpdateBackingTraversalState traversalState;
        Vector<Ref<GraphicsLayer>> childList;
        updateBackingAndHierarchy(*updateRoot, childList, traversalState, scrollingTreeState);

        if (scrollingTreeState.needSynchronousScrollingReasonsUpdate)
            updateSynchronousScrollingNodes();

        // Host the document layer in the RenderView's root layer.
        appendDocumentOverlayLayers(childList);
        // Even when childList is empty, don't drop out of compositing mode if there are
        // composited layers that we didn't hit in our traversal (e.g. because of visibility:hidden).
        if (childList.isEmpty() && !needsCompositingForContentOrOverlays())
            destroyRootLayer();
        else if (m_rootContentsLayer)
            m_rootContentsLayer->setChildren(WTFMove(childList));

        if (scrollingCoordinator && scrollingCoordinator->hasSubscrollers() != hadSubscrollers)
            invalidateEventRegionForAllFrames();
    }

#if !LOG_DISABLED
    if (compositingLogEnabled()) {
        MonotonicTime endTime = MonotonicTime::now();
        LOG(Compositing, "Total layers   primary   secondary   obligatory backing (KB)   secondary backing(KB)   total backing (KB)  update time (ms)\n");

        LOG(Compositing, "%8d %11d %9d %20.2f %22.2f %22.2f %18.2f\n",
            m_obligateCompositedLayerCount + m_secondaryCompositedLayerCount, m_obligateCompositedLayerCount,
            m_secondaryCompositedLayerCount, m_obligatoryBackingStoreBytes / 1024, m_secondaryBackingStoreBytes / 1024, (m_obligatoryBackingStoreBytes + m_secondaryBackingStoreBytes) / 1024, (endTime - startTime).milliseconds());
    }
#endif

    // FIXME: Only do if dirty.
    updateRootLayerPosition();

#if ENABLE(TREE_DEBUGGING)
    if (compositingLogEnabled()) {
        LOG(Compositing, "RenderLayerCompositor::updateCompositingLayers - post");
        showPaintOrderTree(m_renderView.layer());
    }
#endif

    InspectorInstrumentation::layerTreeDidChange(&page());

    if (m_renderView.needsRepaintHackAfterCompositingLayerUpdateForDebugOverlaysOnly()) {
        m_renderView.repaintRootContents();
        m_renderView.setNeedsRepaintHackAfterCompositingLayerUpdateForDebugOverlaysOnly(false);
    }

    return true;
}

void RenderLayerCompositor::computeCompositingRequirements(RenderLayer* ancestorLayer, RenderLayer& layer, LayerOverlapMap& overlapMap, CompositingState& compositingState, BackingSharingState& backingSharingState, bool& descendantHas3DTransform)
{
    layer.updateDescendantDependentFlags();
    layer.updateLayerListsIfNeeded();

    if (!layer.hasDescendantNeedingCompositingRequirementsTraversal()
        && !layer.needsCompositingRequirementsTraversal()
        && !compositingState.fullPaintOrderTraversalRequired
        && !compositingState.descendantsRequireCompositingUpdate) {
        traverseUnchangedSubtree(ancestorLayer, layer, overlapMap, compositingState, backingSharingState, descendantHas3DTransform);
        return;
    }

    LOG_WITH_STREAM(Compositing, stream << TextStream::Repeat(compositingState.depth * 2, ' ') << &layer << (layer.isNormalFlowOnly() ? " n" : " s") << " computeCompositingRequirements (backing provider candidate " << backingSharingState.backingProviderCandidate() << ")");

    // FIXME: maybe we can avoid updating all remaining layers in paint order.
    compositingState.fullPaintOrderTraversalRequired |= layer.needsCompositingRequirementsTraversal();
    compositingState.descendantsRequireCompositingUpdate |= layer.descendantsNeedCompositingRequirementsTraversal();

    layer.setHasCompositingDescendant(false);

    // We updated compositing for direct reasons in layerStyleChanged(). Here, check for compositing that can only be evaluated after layout.
    RequiresCompositingData queryData;
    bool willBeComposited = layer.isComposited();
    bool becameCompositedAfterDescendantTraversal = false;
    IndirectCompositingReason compositingReason = compositingState.subtreeIsCompositing ? IndirectCompositingReason::Stacking : IndirectCompositingReason::None;

    if (layer.needsPostLayoutCompositingUpdate() || compositingState.fullPaintOrderTraversalRequired || compositingState.descendantsRequireCompositingUpdate) {
        layer.setIndirectCompositingReason(IndirectCompositingReason::None);
        willBeComposited = needsToBeComposited(layer, queryData);
    }

    bool layerPaintsIntoProvidedBacking = false;
    if (!willBeComposited && compositingState.subtreeIsCompositing && canBeComposited(layer) && backingSharingState.canIncludeLayer(layer)) {
        backingSharingState.appendSharingLayer(layer);
        LOG_WITH_STREAM(Compositing, stream << " layer " << &layer << " can share with " << backingSharingState.backingProviderCandidate());
        compositingReason = IndirectCompositingReason::None;
        layerPaintsIntoProvidedBacking = true;
    }

    compositingState.fullPaintOrderTraversalRequired |= layer.subsequentLayersNeedCompositingRequirementsTraversal();

    OverlapExtent layerExtent;
    // Use the fact that we're composited as a hint to check for an animating transform.
    // FIXME: Maybe needsToBeComposited() should return a bitmask of reasons, to avoid the need to recompute things.
    if (willBeComposited && !layer.isRenderViewLayer())
        layerExtent.hasTransformAnimation = isRunningTransformAnimation(layer.renderer());

    bool respectTransforms = !layerExtent.hasTransformAnimation;
    overlapMap.geometryMap().pushMappingsToAncestor(&layer, ancestorLayer, respectTransforms);

    // If we know for sure the layer is going to be composited, don't bother looking it up in the overlap map
    if (!willBeComposited && !layerPaintsIntoProvidedBacking && !overlapMap.isEmpty() && compositingState.testingOverlap) {
        // If we're testing for overlap, we only need to composite if we overlap something that is already composited.
        if (layerOverlaps(overlapMap, layer, layerExtent))
            compositingReason = IndirectCompositingReason::Overlap;
        else
            compositingReason = IndirectCompositingReason::None;
    }

#if ENABLE(VIDEO)
    // Video is special. It's the only RenderLayer type that can both have
    // RenderLayer children and whose children can't use its backing to render
    // into. These children (the controls) always need to be promoted into their
    // own layers to draw on top of the accelerated video.
    if (compositingState.compositingAncestor && compositingState.compositingAncestor->renderer().isVideo())
        compositingReason = IndirectCompositingReason::Overlap;
#endif

    if (compositingReason != IndirectCompositingReason::None)
        layer.setIndirectCompositingReason(compositingReason);

    // Check if the computed indirect reason will force the layer to become composited.
    if (!willBeComposited && layer.mustCompositeForIndirectReasons() && canBeComposited(layer)) {
        LOG_WITH_STREAM(Compositing, stream << "layer " << &layer << " compositing for indirect reason " << layer.indirectCompositingReason() << " (was sharing: " << layerPaintsIntoProvidedBacking << ")");
        willBeComposited = true;
        layerPaintsIntoProvidedBacking = false;
    }

    // The children of this layer don't need to composite, unless there is
    // a compositing layer among them, so start by inheriting the compositing
    // ancestor with subtreeIsCompositing set to false.
    CompositingState currentState = compositingState.stateForPaintOrderChildren(layer);
    bool didPushOverlapContainer = false;

    auto layerWillComposite = [&] {
        // This layer is going to be composited, so children can safely ignore the fact that there's an
        // animation running behind this layer, meaning they can rely on the overlap map testing again.
        currentState.testingOverlap = true;
        // This layer now acts as the ancestor for kids.
        currentState.compositingAncestor = &layer;
        // Compositing turns off backing sharing.
        currentState.backingSharingAncestor = nullptr;

        if (layerPaintsIntoProvidedBacking) {
            layerPaintsIntoProvidedBacking = false;
            // layerPaintsIntoProvidedBacking was only true for layers that would otherwise composite because of overlap. If we can
            // no longer share, put this this indirect reason back on the layer so that requiresOwnBackingStore() sees it.
            layer.setIndirectCompositingReason(IndirectCompositingReason::Overlap);
            LOG_WITH_STREAM(Compositing, stream << "layer " << &layer << " was sharing now will composite");
        } else {
            if (!didPushOverlapContainer) {
                overlapMap.pushCompositingContainer(layer);
                didPushOverlapContainer = true;
                LOG_WITH_STREAM(CompositingOverlap, stream << "layer " << &layer << " will composite, pushed container " << overlapMap);
            }
        }

        willBeComposited = true;
    };

    auto layerWillCompositePostDescendants = [&] {
        layerWillComposite();
        currentState.subtreeIsCompositing = true;
        becameCompositedAfterDescendantTraversal = true;
    };

    if (willBeComposited) {
        layerWillComposite();

        computeExtent(overlapMap, layer, layerExtent);
        currentState.ancestorHasTransformAnimation |= layerExtent.hasTransformAnimation;
        // Too hard to compute animated bounds if both us and some ancestor is animating transform.
        layerExtent.animationCausesExtentUncertainty |= layerExtent.hasTransformAnimation && compositingState.ancestorHasTransformAnimation;
    } else if (layerPaintsIntoProvidedBacking) {
        currentState.backingSharingAncestor = &layer;
        overlapMap.pushCompositingContainer(layer);
        didPushOverlapContainer = true;
        LOG_WITH_STREAM(CompositingOverlap, stream << "layer " << &layer << " will share, pushed container " << overlapMap);
    }

    auto preDescendantProviderCandidate = backingSharingState.updateBeforeDescendantTraversal(layer, willBeComposited);

#if ASSERT_ENABLED
    LayerListMutationDetector mutationChecker(layer);
#endif

    bool anyDescendantHas3DTransform = false;
    bool descendantsAddedToOverlap = currentState.hasNonRootCompositedAncestor();

    if (layer.hasNegativeZOrderLayers()) {
        // Speculatively push this layer onto the overlap map.
        bool didSpeculativelyPushOverlapContainer = false;
        if (!didPushOverlapContainer) {
            overlapMap.pushSpeculativeCompositingContainer(layer);
            didPushOverlapContainer = true;
            didSpeculativelyPushOverlapContainer = true;
        }

        for (auto* childLayer : layer.negativeZOrderLayers()) {
            computeCompositingRequirements(&layer, *childLayer, overlapMap, currentState, backingSharingState, anyDescendantHas3DTransform);

            // If we have to make a layer for this child, make one now so we can have a contents layer
            // (since we need to ensure that the -ve z-order child renders underneath our contents).
            if (!willBeComposited && currentState.subtreeIsCompositing) {
                layer.setIndirectCompositingReason(IndirectCompositingReason::BackgroundLayer);
                layerWillComposite();
                overlapMap.confirmSpeculativeCompositingContainer();
            }
        }

        if (didSpeculativelyPushOverlapContainer) {
            if (overlapMap.maybePopSpeculativeCompositingContainer())
                didPushOverlapContainer = false;
            else if (!willBeComposited) {
                layer.setIndirectCompositingReason(IndirectCompositingReason::BackgroundLayer);
                layerWillComposite();
            }
        }
    }

    for (auto* childLayer : layer.normalFlowLayers())
        computeCompositingRequirements(&layer, *childLayer, overlapMap, currentState, backingSharingState, anyDescendantHas3DTransform);

    for (auto* childLayer : layer.positiveZOrderLayers())
        computeCompositingRequirements(&layer, *childLayer, overlapMap, currentState, backingSharingState, anyDescendantHas3DTransform);

    // Set the flag to say that this layer has compositing children.
    layer.setHasCompositingDescendant(currentState.subtreeIsCompositing);
    layer.setHasCompositedNonContainedDescendants(currentState.hasCompositedNonContainedDescendants);

    // If we just entered compositing mode, the root will have become composited (as long as accelerated compositing is enabled).
    if (layer.isRenderViewLayer()) {
        if (usesCompositing() && m_hasAcceleratedCompositing)
            willBeComposited = true;
    }

#if ENABLE(CSS_COMPOSITING)
    bool isolatedCompositedBlending = layer.isolatesCompositedBlending();
    layer.setHasNotIsolatedCompositedBlendingDescendants(currentState.hasNotIsolatedCompositedBlendingDescendants);
    if (layer.isolatesCompositedBlending() != isolatedCompositedBlending) {
        // isolatedCompositedBlending affects the result of clippedByAncestor().
        layer.setChildrenNeedCompositingGeometryUpdate();
    }

    ASSERT(!layer.hasNotIsolatedCompositedBlendingDescendants() || layer.hasNotIsolatedBlendingDescendants());
#endif
    // Now check for reasons to become composited that depend on the state of descendant layers.
    if (!willBeComposited && canBeComposited(layer)) {
        auto indirectReason = computeIndirectCompositingReason(layer, currentState.subtreeIsCompositing, anyDescendantHas3DTransform, layerPaintsIntoProvidedBacking);
        if (indirectReason != IndirectCompositingReason::None) {
            layer.setIndirectCompositingReason(indirectReason);
            layerWillCompositePostDescendants();
        }
    }

    if (layer.reflectionLayer()) {
        // FIXME: Shouldn't we call computeCompositingRequirements to handle a reflection overlapping with another renderer?
        layer.reflectionLayer()->setIndirectCompositingReason(willBeComposited ? IndirectCompositingReason::Stacking : IndirectCompositingReason::None);
    }

    // If we're back at the root, and no other layers need to be composited, and the root layer itself doesn't need
    // to be composited, then we can drop out of compositing mode altogether. However, don't drop out of compositing mode
    // if there are composited layers that we didn't hit in our traversal (e.g. because of visibility:hidden).
    RequiresCompositingData rootLayerQueryData;
    if (layer.isRenderViewLayer() && !currentState.subtreeIsCompositing && !requiresCompositingLayer(layer, rootLayerQueryData) && !m_forceCompositingMode && !needsCompositingForContentOrOverlays()) {
        // Don't drop out of compositing on iOS, because we may flash. See <rdar://problem/8348337>.
#if !PLATFORM(IOS_FAMILY)
        enableCompositingMode(false);
        willBeComposited = false;
#endif
    }

    ASSERT(willBeComposited == needsToBeComposited(layer, queryData));

    // Create or destroy backing here. However, we can't update geometry because layers above us may become composited
    // during post-order traversal (e.g. for clipping).
    if (updateBacking(layer, queryData, &backingSharingState, willBeComposited ? BackingRequired::Yes : BackingRequired::No)) {
        layer.setNeedsCompositingLayerConnection();
        // Child layers need to get a geometry update to recompute their position.
        layer.setChildrenNeedCompositingGeometryUpdate();
        // The composited bounds of enclosing layers depends on which descendants are composited, so they need a geometry update.
        layer.setNeedsCompositingGeometryUpdateOnAncestors();
    }

    // Update layer state bits.
    if (layer.reflectionLayer() && updateLayerCompositingState(*layer.reflectionLayer(), &layer, queryData, backingSharingState))
        layer.setNeedsCompositingLayerConnection();
    
    // FIXME: clarify needsCompositingPaintOrderChildrenUpdate. If a composited layer gets a new ancestor, it needs geometry computations.
    if (layer.needsCompositingPaintOrderChildrenUpdate()) {
        layer.setChildrenNeedCompositingGeometryUpdate();
        layer.setNeedsCompositingLayerConnection();
    }

    layer.clearCompositingRequirementsTraversalState();

    // Compute state passed to the caller.
    descendantHas3DTransform |= anyDescendantHas3DTransform || layer.has3DTransform();
    compositingState.updateWithDescendantStateAndLayer(currentState, layer, ancestorLayer, layerExtent);
    backingSharingState.updateAfterDescendantTraversal(layer, preDescendantProviderCandidate, compositingState.stackingContextAncestor);

    bool layerContributesToOverlap = (currentState.compositingAncestor && !currentState.compositingAncestor->isRenderViewLayer()) || currentState.backingSharingAncestor;
    updateOverlapMap(overlapMap, layer, layerExtent, didPushOverlapContainer, layerContributesToOverlap, becameCompositedAfterDescendantTraversal && !descendantsAddedToOverlap);

    if (layer.isComposited())
        layer.backing()->updateAllowsBackingStoreDetaching(layerExtent.bounds);

    overlapMap.geometryMap().popMappingsToAncestor(ancestorLayer);

    LOG_WITH_STREAM(Compositing, stream << TextStream::Repeat(compositingState.depth * 2, ' ') << &layer << " computeCompositingRequirements - willBeComposited " << willBeComposited << " (backing provider candidate " << backingSharingState.backingProviderCandidate() << ")");
}

// We have to traverse unchanged layers to fill in the overlap map.
void RenderLayerCompositor::traverseUnchangedSubtree(RenderLayer* ancestorLayer, RenderLayer& layer, LayerOverlapMap& overlapMap, CompositingState& compositingState, BackingSharingState& backingSharingState, bool& descendantHas3DTransform)
{
    layer.updateDescendantDependentFlags();
    layer.updateLayerListsIfNeeded();

    ASSERT(!compositingState.fullPaintOrderTraversalRequired);
    ASSERT(!layer.hasDescendantNeedingCompositingRequirementsTraversal());
    ASSERT(!layer.needsCompositingRequirementsTraversal());

    LOG_WITH_STREAM(Compositing, stream << TextStream::Repeat(compositingState.depth * 2, ' ') << &layer << (layer.isNormalFlowOnly() ? " n" : " s") << " traverseUnchangedSubtree");

    bool layerIsComposited = layer.isComposited();
    bool layerPaintsIntoProvidedBacking = false;
    bool didPushOverlapContainer = false;

    OverlapExtent layerExtent;
    if (layerIsComposited && !layer.isRenderViewLayer())
        layerExtent.hasTransformAnimation = isRunningTransformAnimation(layer.renderer());

    bool respectTransforms = !layerExtent.hasTransformAnimation;
    overlapMap.geometryMap().pushMappingsToAncestor(&layer, ancestorLayer, respectTransforms);

    // If we know for sure the layer is going to be composited, don't bother looking it up in the overlap map
    if (!layerIsComposited && !overlapMap.isEmpty() && compositingState.testingOverlap)
        computeExtent(overlapMap, layer, layerExtent);

    if (layer.paintsIntoProvidedBacking()) {
        ASSERT(backingSharingState.canIncludeLayer(layer));
        backingSharingState.appendSharingLayer(layer);
        layerPaintsIntoProvidedBacking = true;
    }

    CompositingState currentState = compositingState.stateForPaintOrderChildren(layer);

    if (layerIsComposited) {
        // This layer is going to be composited, so children can safely ignore the fact that there's an
        // animation running behind this layer, meaning they can rely on the overlap map testing again.
        currentState.testingOverlap = true;
        // This layer now acts as the ancestor for kids.
        currentState.compositingAncestor = &layer;
        currentState.backingSharingAncestor = nullptr;
        overlapMap.pushCompositingContainer(layer);
        didPushOverlapContainer = true;
        LOG_WITH_STREAM(CompositingOverlap, stream << "unchangedSubtree: layer " << &layer << " will composite, pushed container " << overlapMap);

        computeExtent(overlapMap, layer, layerExtent);
        currentState.ancestorHasTransformAnimation |= layerExtent.hasTransformAnimation;
        // Too hard to compute animated bounds if both us and some ancestor is animating transform.
        layerExtent.animationCausesExtentUncertainty |= layerExtent.hasTransformAnimation && compositingState.ancestorHasTransformAnimation;
    } else if (layerPaintsIntoProvidedBacking) {
        overlapMap.pushCompositingContainer(layer);
        currentState.backingSharingAncestor = &layer;
        didPushOverlapContainer = true;
        LOG_WITH_STREAM(CompositingOverlap, stream << "unchangedSubtree: layer " << &layer << " will share, pushed container " << overlapMap);
    }

    auto preDescendantProviderCandidate = backingSharingState.updateBeforeDescendantTraversal(layer, layerIsComposited);

#if ASSERT_ENABLED
    LayerListMutationDetector mutationChecker(layer);
#endif

    bool anyDescendantHas3DTransform = false;

    for (auto* childLayer : layer.negativeZOrderLayers()) {
        traverseUnchangedSubtree(&layer, *childLayer, overlapMap, currentState, backingSharingState, anyDescendantHas3DTransform);
        if (currentState.subtreeIsCompositing)
            ASSERT(layerIsComposited);
    }

    for (auto* childLayer : layer.normalFlowLayers())
        traverseUnchangedSubtree(&layer, *childLayer, overlapMap, currentState, backingSharingState, anyDescendantHas3DTransform);

    for (auto* childLayer : layer.positiveZOrderLayers())
        traverseUnchangedSubtree(&layer, *childLayer, overlapMap, currentState, backingSharingState, anyDescendantHas3DTransform);

    // Set the flag to say that this layer has compositing children.
    ASSERT(layer.hasCompositingDescendant() == currentState.subtreeIsCompositing);
    ASSERT_IMPLIES(canBeComposited(layer) && clipsCompositingDescendants(layer), layerIsComposited);

    descendantHas3DTransform |= anyDescendantHas3DTransform || layer.has3DTransform();

    ASSERT(!currentState.fullPaintOrderTraversalRequired);
    compositingState.updateWithDescendantStateAndLayer(currentState, layer, ancestorLayer, layerExtent, true);
    backingSharingState.updateAfterDescendantTraversal(layer, preDescendantProviderCandidate, compositingState.stackingContextAncestor);

    bool layerContributesToOverlap = (currentState.compositingAncestor && !currentState.compositingAncestor->isRenderViewLayer()) || currentState.backingSharingAncestor;
    updateOverlapMap(overlapMap, layer, layerExtent, didPushOverlapContainer, layerContributesToOverlap);

    overlapMap.geometryMap().popMappingsToAncestor(ancestorLayer);

    ASSERT(!layer.needsCompositingRequirementsTraversal());
}

void RenderLayerCompositor::updateBackingAndHierarchy(RenderLayer& layer, Vector<Ref<GraphicsLayer>>& childLayersOfEnclosingLayer, UpdateBackingTraversalState& traversalState, ScrollingTreeState& scrollingTreeState, OptionSet<UpdateLevel> updateLevel)
{
    layer.updateDescendantDependentFlags();
    layer.updateLayerListsIfNeeded();

    bool layerNeedsUpdate = !updateLevel.isEmpty();
    if (layer.descendantsNeedUpdateBackingAndHierarchyTraversal())
        updateLevel.add(UpdateLevel::AllDescendants);

    ScrollingTreeState scrollingStateForDescendants = scrollingTreeState;
    UpdateBackingTraversalState traversalStateForDescendants = traversalState.stateForDescendants();
    Vector<RenderLayer*> layersClippedByScrollers;
    Vector<RenderLayer*> compositedOverflowScrollLayers;
    
    if (layer.needsScrollingTreeUpdate())
        scrollingTreeState.needSynchronousScrollingReasonsUpdate = true;

    auto* layerBacking = layer.backing();
    if (layerBacking) {
        updateLevel.remove(UpdateLevel::CompositedChildren);

        // We updated the composited bounds in RenderLayerBacking::updateAfterLayout(), but it may have changed
        // based on which descendants are now composited.
        if (layerBacking->updateCompositedBounds()) {
            layer.setNeedsCompositingGeometryUpdate();
            // Our geometry can affect descendants.
            updateLevel.add(UpdateLevel::CompositedChildren);
        }
        
        if (layerNeedsUpdate || layer.needsCompositingConfigurationUpdate()) {
            if (layerBacking->updateConfiguration(traversalState.compositingAncestor)) {
                layerNeedsUpdate = true; // We also need to update geometry.
                layer.setNeedsCompositingLayerConnection();
            }

            layerBacking->updateDebugIndicators(m_showDebugBorders, m_showRepaintCounter);
        }
        
        OptionSet<ScrollingNodeChangeFlags> scrollingNodeChanges = { ScrollingNodeChangeFlags::Layer };
        if (layerNeedsUpdate || layer.needsCompositingGeometryUpdate()) {
            layerBacking->updateGeometry(traversalState.compositingAncestor);
            scrollingNodeChanges.add(ScrollingNodeChangeFlags::LayerGeometry);
        } else if (layer.needsScrollingTreeUpdate())
            scrollingNodeChanges.add(ScrollingNodeChangeFlags::LayerGeometry);

        if (auto* reflection = layer.reflectionLayer()) {
            if (auto* reflectionBacking = reflection->backing()) {
                reflectionBacking->updateCompositedBounds();
                reflectionBacking->updateGeometry(&layer);
                reflectionBacking->updateAfterDescendants();
            }
        }

        if (!layer.parent())
            updateRootLayerPosition();

        // FIXME: do based on dirty flags. Need to do this for changes of geometry, configuration and hierarchy.
        // Need to be careful to do the right thing when a scroll-coordinated layer loses a scroll-coordinated ancestor.
        scrollingStateForDescendants.parentNodeID = updateScrollCoordinationForLayer(layer, traversalState.compositingAncestor, scrollingTreeState, scrollingNodeChanges);
        scrollingStateForDescendants.nextChildIndex = 0;
        
        traversalStateForDescendants.compositingAncestor = &layer;
        traversalStateForDescendants.layersClippedByScrollers = &layersClippedByScrollers;
        traversalStateForDescendants.overflowScrollLayers = &compositedOverflowScrollLayers;

#if !LOG_DISABLED
        logLayerInfo(layer, "updateBackingAndHierarchy", traversalState.depth);
#endif
    }

    if (layer.childrenNeedCompositingGeometryUpdate())
        updateLevel.add(UpdateLevel::CompositedChildren);

    // If this layer has backing, then we are collecting its children, otherwise appending
    // to the compositing child list of an enclosing layer.
    Vector<Ref<GraphicsLayer>> layerChildren;
    auto& childList = layerBacking ? layerChildren : childLayersOfEnclosingLayer;

    bool requireDescendantTraversal = layer.hasDescendantNeedingUpdateBackingOrHierarchyTraversal()
        || (layer.hasCompositingDescendant() && (!layerBacking || layer.needsCompositingLayerConnection() || !updateLevel.isEmpty()));

    bool requiresChildRebuild = layerBacking && layer.needsCompositingLayerConnection() && !layer.hasCompositingDescendant();

#if ASSERT_ENABLED
    LayerListMutationDetector mutationChecker(layer);
#endif

    auto appendForegroundLayerIfNecessary = [&] {
        // If a negative z-order child is compositing, we get a foreground layer which needs to get parented.
        if (layer.negativeZOrderLayers().size()) {
            if (layerBacking && layerBacking->foregroundLayer())
                childList.append(*layerBacking->foregroundLayer());
        }
    };

    if (requireDescendantTraversal) {
        for (auto* renderLayer : layer.negativeZOrderLayers())
            updateBackingAndHierarchy(*renderLayer, childList, traversalStateForDescendants, scrollingStateForDescendants, updateLevel);

        appendForegroundLayerIfNecessary();

        for (auto* renderLayer : layer.normalFlowLayers())
            updateBackingAndHierarchy(*renderLayer, childList, traversalStateForDescendants, scrollingStateForDescendants, updateLevel);
        
        for (auto* renderLayer : layer.positiveZOrderLayers())
            updateBackingAndHierarchy(*renderLayer, childList, traversalStateForDescendants, scrollingStateForDescendants, updateLevel);

        // Pass needSynchronousScrollingReasonsUpdate back up.
        scrollingTreeState.needSynchronousScrollingReasonsUpdate |= scrollingStateForDescendants.needSynchronousScrollingReasonsUpdate;
        if (scrollingTreeState.parentNodeID == scrollingStateForDescendants.parentNodeID)
            scrollingTreeState.nextChildIndex = scrollingStateForDescendants.nextChildIndex;

    } else if (requiresChildRebuild)
        appendForegroundLayerIfNecessary();

    if (layerBacking) {
        if (requireDescendantTraversal || requiresChildRebuild) {
            bool parented = false;
            if (is<RenderWidget>(layer.renderer()))
                parented = parentFrameContentLayers(downcast<RenderWidget>(layer.renderer()));

            if (!parented) {
                // If the layer has a clipping layer the overflow controls layers will be siblings of the clipping layer.
                // Otherwise, the overflow control layers are normal children.
                if (!layerBacking->hasClippingLayer() && !layerBacking->hasScrollingLayer()) {
                    if (auto* overflowControlLayer = layerBacking->overflowControlsContainer())
                        layerChildren.append(*overflowControlLayer);
                }

                adjustOverflowScrollbarContainerLayers(layer, compositedOverflowScrollLayers, layersClippedByScrollers, layerChildren);
                layerBacking->parentForSublayers()->setChildren(WTFMove(layerChildren));
            }
        }

        childLayersOfEnclosingLayer.append(*layerBacking->childForSuperlayers());

        if (layerBacking->hasAncestorClippingLayers() && layerBacking->ancestorClippingStack()->hasAnyScrollingLayers())
            traversalState.layersClippedByScrollers->append(&layer);

        if (layer.hasCompositedScrollableOverflow())
            traversalState.overflowScrollLayers->append(&layer);

        layerBacking->updateAfterDescendants();
    }
    
    layer.clearUpdateBackingOrHierarchyTraversalState();
}

// Finds the set of overflow:scroll layers whose overflow controls hosting layer needs to be reparented,
// to ensure that the scrollbars show on top of positioned content inside the scroller.
void RenderLayerCompositor::adjustOverflowScrollbarContainerLayers(RenderLayer& stackingContextLayer, const Vector<RenderLayer*>& overflowScrollLayers, const Vector<RenderLayer*>& layersClippedByScrollers, Vector<Ref<GraphicsLayer>>& layerChildren)
{
    if (layersClippedByScrollers.isEmpty())
        return;

    HashMap<RenderLayer*, RenderLayer*> overflowScrollToLastContainedLayerMap;

    for (auto* clippedLayer : layersClippedByScrollers) {
        auto* clippingStack = clippedLayer->backing()->ancestorClippingStack();

        for (const auto& stackEntry : clippingStack->stack()) {
            if (!stackEntry.clipData.isOverflowScroll)
                continue;

            if (auto* layer = stackEntry.clipData.clippingLayer.get())
                overflowScrollToLastContainedLayerMap.set(layer, clippedLayer);
        }
    }

    for (auto* overflowScrollingLayer : overflowScrollLayers) {
        auto it = overflowScrollToLastContainedLayerMap.find(overflowScrollingLayer);
        if (it == overflowScrollToLastContainedLayerMap.end())
            continue;
    
        auto* lastContainedDescendant = it->value;
        if (!lastContainedDescendant || !lastContainedDescendant->isComposited())
            continue;

        auto* lastContainedDescendantBacking = lastContainedDescendant->backing();
        auto* overflowBacking = overflowScrollingLayer->backing();
        if (!overflowBacking)
            continue;
        
        auto* overflowContainerLayer = overflowBacking->overflowControlsContainer();
        if (!overflowContainerLayer)
            continue;

        overflowContainerLayer->removeFromParent();

        if (overflowBacking->hasAncestorClippingLayers())
            overflowBacking->ensureOverflowControlsHostLayerAncestorClippingStack(&stackingContextLayer);

        if (auto* overflowControlsAncestorClippingStack = overflowBacking->overflowControlsHostLayerAncestorClippingStack()) {
            overflowControlsAncestorClippingStack->lastLayer()->setChildren({ Ref { *overflowContainerLayer } });
            overflowContainerLayer = overflowControlsAncestorClippingStack->firstLayer();
        }

        auto* lastDescendantGraphicsLayer = lastContainedDescendantBacking->childForSuperlayers();
        auto* overflowScrollerGraphicsLayer = overflowBacking->childForSuperlayers();
        
        std::optional<size_t> lastDescendantLayerIndex;
        std::optional<size_t> scrollerLayerIndex;
        for (size_t i = 0; i < layerChildren.size(); ++i) {
            const auto* graphicsLayer = layerChildren[i].ptr();
            if (graphicsLayer == lastDescendantGraphicsLayer)
                lastDescendantLayerIndex = i;
            else if (graphicsLayer == overflowScrollerGraphicsLayer)
                scrollerLayerIndex = i;
        }

        if (lastDescendantLayerIndex && scrollerLayerIndex) {
            auto insertionIndex = std::max(lastDescendantLayerIndex.value() + 1, scrollerLayerIndex.value() + 1);
            LOG_WITH_STREAM(Compositing, stream << "Moving overflow controls layer for " << overflowScrollingLayer << " to appear after " << lastContainedDescendant);
            layerChildren.insert(insertionIndex, *overflowContainerLayer);
        }

        overflowBacking->adjustOverflowControlsPositionRelativeToAncestor(stackingContextLayer);
    }
}

void RenderLayerCompositor::appendDocumentOverlayLayers(Vector<Ref<GraphicsLayer>>& childList)
{
    if (!isMainFrameCompositor() || !m_compositing)
        return;

    if (!page().pageOverlayController().hasDocumentOverlays())
        return;

    Ref<GraphicsLayer> overlayHost = page().pageOverlayController().layerWithDocumentOverlays();
    childList.append(WTFMove(overlayHost));
}

bool RenderLayerCompositor::needsCompositingForContentOrOverlays() const
{
    return m_contentLayersCount + page().pageOverlayController().overlayCount();
}

void RenderLayerCompositor::layerBecameComposited(const RenderLayer& layer)
{
    if (&layer != m_renderView.layer())
        ++m_contentLayersCount;
}

void RenderLayerCompositor::layerBecameNonComposited(const RenderLayer& layer)
{
    // Inform the inspector that the given RenderLayer was destroyed.
    // FIXME: "destroyed" is a misnomer.
    InspectorInstrumentation::renderLayerDestroyed(&page(), layer);

    if (&layer != m_renderView.layer()) {
        ASSERT(m_contentLayersCount > 0);
        --m_contentLayersCount;
    }
}

#if !LOG_DISABLED
void RenderLayerCompositor::logLayerInfo(const RenderLayer& layer, const char* phase, int depth)
{
    if (!compositingLogEnabled())
        return;

    auto* backing = layer.backing();
    RequiresCompositingData queryData;
    if (requiresCompositingLayer(layer, queryData) || layer.isRenderViewLayer()) {
        ++m_obligateCompositedLayerCount;
        m_obligatoryBackingStoreBytes += backing->backingStoreMemoryEstimate();
    } else {
        ++m_secondaryCompositedLayerCount;
        m_secondaryBackingStoreBytes += backing->backingStoreMemoryEstimate();
    }

    LayoutRect absoluteBounds = backing->compositedBounds();
    absoluteBounds.move(layer.offsetFromAncestor(m_renderView.layer()));
    
    StringBuilder logString;
    logString.append(pad(' ', 12 + depth * 2, hex(reinterpret_cast<uintptr_t>(&layer), Lowercase)), " id ", backing->graphicsLayer()->primaryLayerID(), " (", absoluteBounds.x().toFloat(), ',', absoluteBounds.y().toFloat(), '-', absoluteBounds.maxX().toFloat(), ',', absoluteBounds.maxY().toFloat(), ") ", FormattedNumber::fixedWidth(backing->backingStoreMemoryEstimate() / 1024, 2), "KB");

    if (!layer.renderer().style().hasAutoUsedZIndex())
        logString.append(" z-index: ", layer.renderer().style().usedZIndex());

    logString.append(" (", logOneReasonForCompositing(layer), ") ");

    if (backing->graphicsLayer()->contentsOpaque() || backing->paintsIntoCompositedAncestor() || backing->foregroundLayer() || backing->backgroundLayer()) {
        logString.append('[');

        const char* prefix = "";
        if (backing->graphicsLayer()->contentsOpaque()) {
            logString.append("opaque");
            prefix = ", ";
        }

        if (backing->paintsIntoCompositedAncestor()) {
            logString.append(prefix, "paints into ancestor");
            prefix = ", ";
        }

        if (backing->foregroundLayer() || backing->backgroundLayer()) {
            if (backing->foregroundLayer() && backing->backgroundLayer()) {
                logString.append(prefix, "+foreground+background");
                prefix = ", ";
            } else if (backing->foregroundLayer()) {
                logString.append(prefix, "+foreground");
                prefix = ", ";
            } else {
                logString.append(prefix, "+background");
                prefix = ", ";
            }
        }

        logString.append("] ");
    }

    logString.append(layer.name(), " - ", phase);

    LOG(Compositing, "%s", logString.toString().utf8().data());
}
#endif

static bool clippingChanged(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    return oldStyle.overflowX() != newStyle.overflowX() || oldStyle.overflowY() != newStyle.overflowY()
        || oldStyle.hasClip() != newStyle.hasClip() || oldStyle.clip() != newStyle.clip();
}

static bool styleAffectsLayerGeometry(const RenderStyle& style)
{
    return style.hasClip() || style.clipPath() || style.hasBorderRadius();
}

static bool recompositeChangeRequiresGeometryUpdate(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    return oldStyle.transform() != newStyle.transform()
        || oldStyle.translate() != newStyle.translate()
        || oldStyle.scale() != newStyle.scale()
        || oldStyle.rotate() != newStyle.rotate()
        || oldStyle.transformBox() != newStyle.transformBox()
        || oldStyle.transformOriginX() != newStyle.transformOriginX()
        || oldStyle.transformOriginY() != newStyle.transformOriginY()
        || oldStyle.transformOriginZ() != newStyle.transformOriginZ()
        || oldStyle.usedTransformStyle3D() != newStyle.usedTransformStyle3D()
        || oldStyle.perspective() != newStyle.perspective()
        || oldStyle.perspectiveOriginX() != newStyle.perspectiveOriginX()
        || oldStyle.perspectiveOriginY() != newStyle.perspectiveOriginY()
        || oldStyle.backfaceVisibility() != newStyle.backfaceVisibility()
        || !arePointingToEqualData(oldStyle.offsetPath(), newStyle.offsetPath())
        || oldStyle.offsetAnchor() != newStyle.offsetAnchor()
        || oldStyle.offsetPosition() != newStyle.offsetPosition()
        || oldStyle.offsetDistance() != newStyle.offsetDistance()
        || oldStyle.offsetRotate() != newStyle.offsetRotate()
        || !arePointingToEqualData(oldStyle.clipPath(), newStyle.clipPath())
        || oldStyle.overscrollBehaviorX() != newStyle.overscrollBehaviorX()
        || oldStyle.overscrollBehaviorY() != newStyle.overscrollBehaviorY();
}

static bool recompositeChangeRequiresChildrenGeometryUpdate(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    return oldStyle.hasPerspective() != newStyle.hasPerspective()
        || oldStyle.usedTransformStyle3D() != newStyle.usedTransformStyle3D();
}

void RenderLayerCompositor::layerStyleChanged(StyleDifference diff, RenderLayer& layer, const RenderStyle* oldStyle)
{
    if (diff == StyleDifference::Equal)
        return;

    // Create or destroy backing here so that code that runs during layout can reliably use isComposited() (though this
    // is only true for layers composited for direct reasons).
    // Also, it allows us to avoid a tree walk in updateCompositingLayers() when no layer changed its compositing state.
    RequiresCompositingData queryData;
    queryData.layoutUpToDate = LayoutUpToDate::No;
    
    bool layerChanged = updateBacking(layer, queryData);
    if (layerChanged) {
        layer.setChildrenNeedCompositingGeometryUpdate();
        layer.setNeedsCompositingLayerConnection();
        layer.setSubsequentLayersNeedCompositingRequirementsTraversal();
        // Ancestor layers that composited for indirect reasons (things listed in styleChangeMayAffectIndirectCompositingReasons()) need to get updated.
        // This could be optimized by only setting this flag on layers with the relevant styles.
        layer.setNeedsPostLayoutCompositingUpdateOnAncestors();
    }
    
    if (queryData.reevaluateAfterLayout)
        layer.setNeedsPostLayoutCompositingUpdate();

    const auto& newStyle = layer.renderer().style();

    if (hasContentCompositingLayers()) {
        if (diff >= StyleDifference::LayoutPositionedMovementOnly) {
            layer.setNeedsPostLayoutCompositingUpdate();
            layer.setNeedsCompositingGeometryUpdate();
        }

        if (diff >= StyleDifference::Layout) {
            // FIXME: only set flags here if we know we have a composited descendant, but we might not know at this point.
            if (oldStyle && clippingChanged(*oldStyle, newStyle)) {
                if (layer.isStackingContext()) {
                    layer.setNeedsPostLayoutCompositingUpdate(); // Layer needs to become composited if it has composited descendants.
                    layer.setNeedsCompositingConfigurationUpdate(); // If already composited, layer needs to create/destroy clipping layer.
                    layer.setChildrenNeedCompositingGeometryUpdate(); // Clipping layers on this layer affect descendant layer geometry.
                } else {
                    // Descendant (in containing block order) compositing layers need to re-evaluate their clipping,
                    // but they might be siblings in z-order so go up to our stacking context.
                    if (auto* stackingContext = layer.stackingContext())
                        stackingContext->setDescendantsNeedUpdateBackingAndHierarchyTraversal();
                }
            }

            // This ensures that the viewport anchor layer will be updated when updating compositing layers upon style change
            auto styleChangeAffectsAnchorLayer = [](const RenderStyle* oldStyle, const RenderStyle& newStyle) {
                if (!oldStyle)
                    return false;

                return oldStyle->hasViewportConstrainedPosition() != newStyle.hasViewportConstrainedPosition();
            };

            if (styleChangeAffectsAnchorLayer(oldStyle, newStyle))
                layer.setNeedsCompositingConfigurationUpdate();

            // These properties trigger compositing if some descendant is composited.
            if (oldStyle && styleChangeMayAffectIndirectCompositingReasons(*oldStyle, newStyle))
                layer.setNeedsPostLayoutCompositingUpdate();

            layer.setNeedsCompositingGeometryUpdate();
        }
    }

    if (diff >= StyleDifference::Repaint && oldStyle) {
        // This ensures that we update border-radius clips on layers that are descendants in containing-block order but not paint order. This is necessary even when
        // the current layer is not composited.
        bool changeAffectsClippingOfNonPaintOrderDescendants = !layer.isStackingContext() && layer.renderer().hasNonVisibleOverflow() && oldStyle->border() != newStyle.border();
        if (changeAffectsClippingOfNonPaintOrderDescendants) {
            if (auto* parent = layer.paintOrderParent())
                parent->setChildrenNeedCompositingGeometryUpdate();
        }
    }

    auto* backing = layer.backing();
    if (!backing)
        return;

    backing->updateConfigurationAfterStyleChange();

    if (diff >= StyleDifference::Repaint) {
        // Visibility change may affect geometry of the enclosing composited layer.
        if (oldStyle && oldStyle->visibility() != newStyle.visibility())
            layer.setNeedsCompositingGeometryUpdate();
        
        // We'll get a diff of Repaint when things like clip-path change; these might affect layer or inner-layer geometry.
        if (layer.isComposited() && oldStyle) {
            if (styleAffectsLayerGeometry(*oldStyle) || styleAffectsLayerGeometry(newStyle))
                layer.setNeedsCompositingGeometryUpdate();
        }

        // image rendering mode can determine whether we use device pixel ratio for the backing store.
        if (oldStyle && oldStyle->imageRendering() != newStyle.imageRendering())
            layer.setNeedsCompositingConfigurationUpdate();
    }

    if (diff >= StyleDifference::RecompositeLayer) {
        if (layer.isComposited()) {
            bool hitTestingStateChanged = oldStyle && (oldStyle->effectivePointerEvents() != newStyle.effectivePointerEvents());
            if (is<RenderWidget>(layer.renderer()) || hitTestingStateChanged) {
                // For RenderWidgets this is necessary to get iframe layers hooked up in response to scheduleInvalidateStyleAndLayerComposition().
                layer.setNeedsCompositingConfigurationUpdate();
            }
            // If we're changing to/from 0 opacity, then we need to reconfigure the layer since we try to
            // skip backing store allocation for opacity:0.
            if (oldStyle && oldStyle->opacity() != newStyle.opacity() && (!oldStyle->opacity() || !newStyle.opacity()))
                layer.setNeedsCompositingConfigurationUpdate();
        }
        if (oldStyle && recompositeChangeRequiresGeometryUpdate(*oldStyle, newStyle)) {
            // FIXME: transform changes really need to trigger layout. See RenderElement::adjustStyleDifference().
            layer.setNeedsPostLayoutCompositingUpdate();
            layer.setNeedsCompositingGeometryUpdate();
        }
        if (m_renderView.settings().css3DTransformInteroperabilityEnabled() && oldStyle && recompositeChangeRequiresChildrenGeometryUpdate(*oldStyle, newStyle))
            layer.setChildrenNeedCompositingGeometryUpdate();
    }
}

// FIXME: remove and never ask questions about reflection layers.
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
    m_rootContentsLayer->setMasksToBounds(!m_renderView.settings().backgroundShouldExtendBeyondPage());
}

bool RenderLayerCompositor::updateBacking(RenderLayer& layer, RequiresCompositingData& queryData, BackingSharingState* backingSharingState, BackingRequired backingRequired)
{
    bool layerChanged = false;
    if (backingRequired == BackingRequired::Unknown)
        backingRequired = needsToBeComposited(layer, queryData) ? BackingRequired::Yes : BackingRequired::No;
    else {
        // Need to fetch viewportConstrainedNotCompositedReason, but without doing all the work that needsToBeComposited does.
        requiresCompositingForPosition(rendererForCompositingTests(layer), layer, queryData);
    }

    auto repaintTargetsSharedBacking = [&](RenderLayer& layer) {
        return backingSharingState && layerRepaintTargetsBackingSharingLayer(layer, *backingSharingState);
    };
    
    auto repaintLayer = [&](RenderLayer& layer) {
        if (repaintTargetsSharedBacking(layer)) {
            LOG_WITH_STREAM(Compositing, stream << "Layer " << &layer << " needs to repaint into potential backing-sharing layer, postponing repaint");
            backingSharingState->addLayerNeedingRepaint(layer);
        } else
            repaintOnCompositingChange(layer);
    };

    if (backingRequired == BackingRequired::Yes) {
        // If we need to repaint, do so before making backing and disconnecting from the backing provider layer.
        if (!layer.backing())
            repaintLayer(layer);

        layer.disconnectFromBackingProviderLayer();

        enableCompositingMode();
        
        if (!layer.backing()) {
            layer.ensureBacking();

            if (layer.isRenderViewLayer() && useCoordinatedScrollingForLayer(layer)) {
                auto& frameView = m_renderView.frameView();
                if (auto* scrollingCoordinator = this->scrollingCoordinator())
                    scrollingCoordinator->frameViewRootLayerDidChange(frameView);
#if HAVE(RUBBER_BANDING)
                updateLayerForHeader(frameView.headerHeight());
                updateLayerForFooter(frameView.footerHeight());
#endif
                updateRootContentLayerClipping();

                if (auto* tiledBacking = layer.backing()->tiledBacking())
                    tiledBacking->setTopContentInset(frameView.topContentInset());
            }

            // This layer and all of its descendants have cached repaints rects that are relative to
            // the repaint container, so change when compositing changes; we need to update them here.
            if (layer.parent())
                layer.computeRepaintRectsIncludingDescendants();

            layer.setNeedsCompositingGeometryUpdate();
            layer.setNeedsCompositingConfigurationUpdate();
            layer.setNeedsCompositingPaintOrderChildrenUpdate();

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

            layer.clearBacking();
            layerChanged = true;

            // This layer and all of its descendants have cached repaints rects that are relative to
            // the repaint container, so change when compositing changes; we need to update them here,
            // as long as shared backing isn't going to change our repaint container.
            if (!repaintTargetsSharedBacking(layer))
                layer.computeRepaintRectsIncludingDescendants();

            // If we need to repaint, do so now that we've removed the backing.
            repaintLayer(layer);
        }
    }

#if ENABLE(VIDEO)
    if (layerChanged && is<RenderVideo>(layer.renderer())) {
        // If it's a video, give the media player a chance to hook up to the layer.
        downcast<RenderVideo>(layer.renderer()).acceleratedRenderingStateChanged();
    }
#endif

    if (layerChanged && is<RenderWidget>(layer.renderer())) {
        auto* innerCompositor = frameContentsCompositor(downcast<RenderWidget>(layer.renderer()));
        if (innerCompositor && innerCompositor->usesCompositing())
            innerCompositor->updateRootLayerAttachment();
    }
    
    if (layerChanged)
        layer.clearClipRectsIncludingDescendants(PaintingClipRects);

    // If a fixed position layer gained/lost a backing or the reason not compositing it changed,
    // the scrolling coordinator needs to recalculate whether it can do fast scrolling.
    if (layer.renderer().isFixedPositioned()) {
        if (layer.viewportConstrainedNotCompositedReason() != queryData.nonCompositedForPositionReason) {
            layer.setViewportConstrainedNotCompositedReason(queryData.nonCompositedForPositionReason);
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

bool RenderLayerCompositor::updateLayerCompositingState(RenderLayer& layer, const RenderLayer* compositingAncestor, RequiresCompositingData& queryData, BackingSharingState& backingSharingState)
{
    bool layerChanged = updateBacking(layer, queryData, &backingSharingState);

    // See if we need content or clipping layers. Methods called here should assume
    // that the compositing state of descendant layers has not been updated yet.
    if (layer.backing() && layer.backing()->updateConfiguration(compositingAncestor))
        layerChanged = true;

    return layerChanged;
}

void RenderLayerCompositor::repaintOnCompositingChange(RenderLayer& layer)
{
    // If the renderer is not attached yet, no need to repaint.
    if (&layer.renderer() != &m_renderView && !layer.renderer().parent())
        return;

    auto* repaintContainer = layer.renderer().containerForRepaint().renderer;
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
    auto* compositedAncestor = layer.enclosingCompositingLayerForRepaint(ExcludeSelf).layer;
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

void RenderLayerCompositor::layerWillBeRemoved(RenderLayer& parent, RenderLayer& child)
{
    if (parent.renderer().renderTreeBeingDestroyed())
        return;

    if (child.isComposited())
        repaintInCompositedAncestor(child, child.backing()->compositedBounds()); // FIXME: do via dirty bits?
    else if (child.paintsIntoProvidedBacking()) {
        auto* backingProviderLayer = child.backingProviderLayer();
        // FIXME: Optimize this repaint.
        backingProviderLayer->setBackingNeedsRepaint();
        backingProviderLayer->backing()->removeBackingSharingLayer(child);
    } else
        return;

    child.setNeedsCompositingLayerConnection();
}

RenderLayer* RenderLayerCompositor::enclosingNonStackingClippingLayer(const RenderLayer& layer) const
{
    for (auto* parent = layer.parent(); parent; parent = parent->parent()) {
        if (parent->isStackingContext())
            return nullptr;
        if (parent->renderer().hasClipOrNonVisibleOverflow())
            return parent;
    }
    return nullptr;
}

void RenderLayerCompositor::computeExtent(const LayerOverlapMap& overlapMap, const RenderLayer& layer, OverlapExtent& extent) const
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

enum class AncestorTraversal { Continue, Stop };

// This is a simplified version of containing block walking that only handles absolute position.
template <typename Function>
static AncestorTraversal traverseAncestorLayers(const RenderLayer& layer, Function&& function)
{
    bool containingBlockCanSkipLayers = layer.renderer().isAbsolutelyPositioned();
    RenderLayer* nextPaintOrderParent = layer.paintOrderParent();

    for (const auto* ancestorLayer = layer.parent(); ancestorLayer; ancestorLayer = ancestorLayer->parent()) {
        bool inContainingBlockChain = true;

        if (containingBlockCanSkipLayers)
            inContainingBlockChain = ancestorLayer->renderer().canContainAbsolutelyPositionedObjects();

        if (function(*ancestorLayer, inContainingBlockChain, ancestorLayer == nextPaintOrderParent) == AncestorTraversal::Stop)
            return AncestorTraversal::Stop;

        if (inContainingBlockChain)
            containingBlockCanSkipLayers = ancestorLayer->renderer().isAbsolutelyPositioned();
        
        if (ancestorLayer == nextPaintOrderParent)
            nextPaintOrderParent = ancestorLayer->paintOrderParent();
    }
    
    return AncestorTraversal::Continue;
}

static bool createsClippingScope(const RenderLayer& layer)
{
    return layer.hasCompositedScrollableOverflow();
}

static Vector<LayerOverlapMap::LayerAndBounds> enclosingClippingScopes(const RenderLayer& layer, const RenderLayer& rootLayer)
{
    Vector<LayerOverlapMap::LayerAndBounds> clippingScopes;
    clippingScopes.append({ const_cast<RenderLayer&>(rootLayer), { } });

    if (!layer.hasCompositedScrollingAncestor())
        return clippingScopes;

    traverseAncestorLayers(layer, [&](const RenderLayer& ancestorLayer, bool inContainingBlockChain, bool) {
        if (inContainingBlockChain && createsClippingScope(ancestorLayer)) {
            LayoutRect clipRect;
            if (is<RenderBox>(ancestorLayer.renderer())) {
                // FIXME: This is expensive. Broken with transforms.
                LayoutPoint offsetFromRoot = ancestorLayer.convertToLayerCoords(&rootLayer, { });
                clipRect = downcast<RenderBox>(ancestorLayer.renderer()).overflowClipRect(offsetFromRoot);
            }

            LayerOverlapMap::LayerAndBounds layerAndBounds { const_cast<RenderLayer&>(ancestorLayer), clipRect };
            clippingScopes.insert(1, layerAndBounds); // Order is roots to leaves.
        }
        return AncestorTraversal::Continue;
    });

    return clippingScopes;
}

void RenderLayerCompositor::addToOverlapMap(LayerOverlapMap& overlapMap, const RenderLayer& layer, OverlapExtent& extent) const
{
    if (layer.isRenderViewLayer())
        return;

    computeExtent(overlapMap, layer, extent);

    // FIXME: constrain the scopes (by composited stacking context ancestor I think).
    auto clippingScopes = enclosingClippingScopes(layer, rootRenderLayer());

    LayoutRect clipRect;
    if (layer.hasCompositedScrollingAncestor()) {
        // Compute a clip up to the composited scrolling ancestor, then convert it to absolute coordinates.
        auto& scrollingScope = clippingScopes.last();
        auto& scopeLayer = scrollingScope.layer;
        clipRect = layer.backgroundClipRect(RenderLayer::ClipRectsContext(&scopeLayer, TemporaryClipRects, { })).rect();
        if (!clipRect.isInfinite())
            clipRect.setLocation(scopeLayer.convertToLayerCoords(&rootRenderLayer(), clipRect.location()));
    } else
        clipRect = layer.backgroundClipRect(RenderLayer::ClipRectsContext(&rootRenderLayer(), AbsoluteClipRects)).rect(); // FIXME: Incorrect for CSS regions.

    auto clippedBounds = extent.bounds;
    if (!clipRect.isInfinite()) {
        // With delegated page scaling, pageScaleFactor() is not applied by RenderView, so we should not scale here.
        if (!page().delegatesScaling())
            clipRect.scale(pageScaleFactor());

        clippedBounds.intersect(clipRect);
    }

    overlapMap.add(layer, clippedBounds, clippingScopes);
}

void RenderLayerCompositor::addDescendantsToOverlapMapRecursive(LayerOverlapMap& overlapMap, const RenderLayer& layer, const RenderLayer* ancestorLayer) const
{
    if (!canBeComposited(layer))
        return;

    // A null ancestorLayer is an indication that 'layer' has already been pushed.
    if (ancestorLayer) {
        overlapMap.geometryMap().pushMappingsToAncestor(&layer, ancestorLayer);
    
        OverlapExtent layerExtent;
        addToOverlapMap(overlapMap, layer, layerExtent);
    }

#if ASSERT_ENABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer&>(layer));
#endif

    for (auto* renderLayer : layer.negativeZOrderLayers())
        addDescendantsToOverlapMapRecursive(overlapMap, *renderLayer, &layer);

    for (auto* renderLayer : layer.normalFlowLayers())
        addDescendantsToOverlapMapRecursive(overlapMap, *renderLayer, &layer);

    for (auto* renderLayer : layer.positiveZOrderLayers())
        addDescendantsToOverlapMapRecursive(overlapMap, *renderLayer, &layer);
    
    if (ancestorLayer)
        overlapMap.geometryMap().popMappingsToAncestor(ancestorLayer);
}

void RenderLayerCompositor::updateOverlapMap(LayerOverlapMap& overlapMap, const RenderLayer& layer, OverlapExtent& layerExtent, bool didPushContainer, bool addLayerToOverlap, bool addDescendantsToOverlap) const
{
    if (addLayerToOverlap)
        addToOverlapMap(overlapMap, layer, layerExtent);

    if (addDescendantsToOverlap) {
        // If this is the first non-root layer to composite, we need to add all the descendants we already traversed to the overlap map.
        addDescendantsToOverlapMapRecursive(overlapMap, layer);
        LOG_WITH_STREAM(CompositingOverlap, stream << "layer " << &layer << " composited post descendant traversal, added recursive " << overlapMap);
    }

    if (didPushContainer) {
        overlapMap.popCompositingContainer(layer);
        LOG_WITH_STREAM(CompositingOverlap, stream << "layer " << &layer << " is composited or shared, popped container " << overlapMap);
    }
}

bool RenderLayerCompositor::layerOverlaps(const LayerOverlapMap& overlapMap, const RenderLayer& layer, OverlapExtent& layerExtent) const
{
    computeExtent(overlapMap, layer, layerExtent);

    auto clippingScopes = enclosingClippingScopes(layer, rootRenderLayer());
    return overlapMap.overlapsLayers(layer, layerExtent.bounds, clippingScopes);
}

#if ENABLE(VIDEO)
bool RenderLayerCompositor::canAccelerateVideoRendering(RenderVideo& video) const
{
    if (!m_hasAcceleratedCompositing)
        return false;

    return video.supportsAcceleratedRendering();
}
#endif

void RenderLayerCompositor::frameViewDidChangeLocation(const IntPoint& contentsOffset)
{
    if (m_overflowControlsHostLayer)
        m_overflowControlsHostLayer->setPosition(contentsOffset);
}

void RenderLayerCompositor::frameViewDidChangeSize()
{
    if (auto* layer = m_renderView.layer())
        layer->setNeedsCompositingGeometryUpdate();

    if (m_scrolledContentsLayer) {
        updateScrollLayerClipping();
        frameViewDidScroll();
        updateOverflowControlsLayers();

#if HAVE(RUBBER_BANDING)
        if (m_layerForOverhangAreas) {
            auto& frameView = m_renderView.frameView();
            m_layerForOverhangAreas->setSize(frameView.frameRect().size());
            m_layerForOverhangAreas->setPosition(FloatPoint(0, frameView.topContentInset()));
        }
#endif
    }
}

void RenderLayerCompositor::widgetDidChangeSize(RenderWidget& widget)
{
    if (!widget.hasLayer())
        return;

    auto& layer = *widget.layer();

    LOG_WITH_STREAM(Compositing, stream << "RenderLayerCompositor " << this << " widgetDidChangeSize (layer " << &layer << ")");

    // Widget size affects answer to requiresCompositingForFrame() so we need to trigger
    // a compositing update.
    layer.setNeedsPostLayoutCompositingUpdate();
    scheduleCompositingLayerUpdate();

    if (layer.isComposited())
        layer.backing()->updateAfterWidgetResize();
}

bool RenderLayerCompositor::hasCoordinatedScrolling() const
{
    auto* scrollingCoordinator = this->scrollingCoordinator();
    return scrollingCoordinator && scrollingCoordinator->coordinatesScrollingForFrameView(m_renderView.frameView());
}

void RenderLayerCompositor::updateScrollLayerPosition()
{
    ASSERT(!hasCoordinatedScrolling());
    ASSERT(m_scrolledContentsLayer);

    auto& frameView = m_renderView.frameView();
    IntPoint scrollPosition = frameView.scrollPosition();

    // We use scroll position here because the root content layer is offset to account for scrollOrigin (see FrameView::positionForRootContentLayer).
    m_scrolledContentsLayer->setPosition(FloatPoint(-scrollPosition.x(), -scrollPosition.y()));

    if (auto* fixedBackgroundLayer = fixedRootBackgroundLayer())
        fixedBackgroundLayer->setPosition(frameView.scrollPositionForFixedPosition());
}

void RenderLayerCompositor::updateScrollLayerClipping()
{
    auto* layerForClipping = this->layerForClipping();
    if (!layerForClipping)
        return;

    auto layerSize = m_renderView.frameView().sizeForVisibleContent();
    layerForClipping->setSize(layerSize);
    layerForClipping->setPosition(positionForClipLayer());

#if ENABLE(SCROLLING_THREAD)
    if (layerForClipping == m_clipLayer) {
        EventRegion eventRegion;
        auto eventRegionContext = eventRegion.makeContext();
        eventRegionContext.unite(IntRect({ }, layerSize), m_renderView, RenderStyle::defaultStyle());
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        eventRegionContext.copyInteractionRegionsToEventRegion();
#endif
        m_clipLayer->setEventRegion(WTFMove(eventRegion));
    }
#endif
}

FloatPoint RenderLayerCompositor::positionForClipLayer() const
{
    auto& frameView = m_renderView.frameView();

    return FloatPoint(
        frameView.shouldPlaceVerticalScrollbarOnLeft() ? frameView.horizontalScrollbarIntrusion() : 0,
        FrameView::yPositionForInsetClipLayer(frameView.scrollPosition(), frameView.topContentInset()));
}

void RenderLayerCompositor::frameViewDidScroll()
{
    if (!m_scrolledContentsLayer)
        return;

    // If there's a scrolling coordinator that manages scrolling for this frame view,
    // it will also manage updating the scroll layer position.
    if (hasCoordinatedScrolling()) {
        // We have to schedule a flush in order for the main TiledBacking to update its tile coverage.
        scheduleRenderingUpdate();
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

void RenderLayerCompositor::rootLayerConfigurationChanged()
{
    auto* renderViewBacking = m_renderView.layer()->backing();
    if (renderViewBacking && renderViewBacking->isFrameLayerWithTiledBacking()) {
        m_renderView.layer()->setNeedsCompositingConfigurationUpdate();
        scheduleCompositingLayerUpdate();
    }
}

void RenderLayerCompositor::updateCompositingForLayerTreeAsTextDump()
{
    auto& frameView = m_renderView.frameView();
    frameView.updateLayoutAndStyleIfNeededRecursive();
    
    updateEventRegions();

    for (auto* child = frameView.frame().tree().firstRenderedChild(); child; child = child->tree().traverseNextRendered()) {
        auto* localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        if (auto* renderer = localChild->contentRenderer())
            renderer->compositor().updateEventRegions();
    }

    updateCompositingLayers(CompositingUpdateType::AfterLayout);

    if (!m_rootContentsLayer)
        return;

    flushPendingLayerChanges(true);
    // We need to trigger an update because the flushPendingLayerChanges() will have pushed changes to platform layers,
    // which may cause painting to happen in the current runloop.
    page().triggerRenderingUpdateForTesting();
}

String RenderLayerCompositor::layerTreeAsText(OptionSet<LayerTreeAsTextOptions> options)
{
    LOG_WITH_STREAM(Compositing, stream << "RenderLayerCompositor " << this << " layerTreeAsText");

    updateCompositingForLayerTreeAsTextDump();

    if (!m_rootContentsLayer)
        return String();

    // We skip dumping the scroll and clip layers to keep layerTreeAsText output
    // similar between platforms.
    String layerTreeText = m_rootContentsLayer->layerTreeAsText(options);

    // Dump an empty layer tree only if the only composited layer is the main frame's tiled backing,
    // so that tests expecting us to drop out of accelerated compositing when there are no layers succeed.
    if (!hasContentCompositingLayers() && documentUsesTiledBacking() && !(options & LayerTreeAsTextOptions::IncludeTileCaches) && !(options & LayerTreeAsTextOptions::IncludeRootLayerProperties))
        layerTreeText = emptyString();

    // The true root layer is not included in the dump, so if we want to report
    // its repaint rects, they must be included here.
    if (options & LayerTreeAsTextOptions::IncludeRepaintRects)
        return m_renderView.frameView().trackedRepaintRectsAsText() + layerTreeText;

    return layerTreeText;
}

std::optional<String> RenderLayerCompositor::platformLayerTreeAsText(Element& element, OptionSet<PlatformLayerTreeAsTextFlags> flags)
{
    LOG_WITH_STREAM(Compositing, stream << "RenderLayerCompositor " << this << " platformLayerTreeAsText");

    updateCompositingForLayerTreeAsTextDump();
    if (!element.renderer() || !element.renderer()->hasLayer())
        return std::nullopt;

    auto& layerModelObject = downcast<RenderLayerModelObject>(*element.renderer());
    if (!layerModelObject.layer()->isComposited())
        return std::nullopt;

    auto* backing = layerModelObject.layer()->backing();
    return backing->graphicsLayer()->platformLayerTreeAsText(flags);
}

static RenderView* frameContentsRenderView(RenderWidget& renderer)
{
    if (auto* contentDocument = renderer.frameOwnerElement().contentDocument())
        return contentDocument->renderView();

    return nullptr;
}

RenderLayerCompositor* RenderLayerCompositor::frameContentsCompositor(RenderWidget& renderer)
{
    if (auto* view = frameContentsRenderView(renderer))
        return &view->compositor();

    return nullptr;
}

bool RenderLayerCompositor::parentFrameContentLayers(RenderWidget& renderer)
{
    auto* innerCompositor = frameContentsCompositor(renderer);
    if (!innerCompositor || !innerCompositor->usesCompositing() || innerCompositor->rootLayerAttachment() != RootLayerAttachedViaEnclosingFrame)
        return false;
    
    auto* layer = renderer.layer();
    if (!layer->isComposited())
        return false;

    auto* backing = layer->backing();
    auto* hostingLayer = backing->parentForSublayers();
    auto* rootLayer = innerCompositor->rootGraphicsLayer();
    if (hostingLayer->children().size() != 1 || hostingLayer->children()[0].ptr() != rootLayer) {
        hostingLayer->removeAllChildren();
        hostingLayer->addChild(*rootLayer);
    }

    if (auto frameHostingNodeID = backing->scrollingNodeIDForRole(ScrollCoordinationRole::FrameHosting)) {
        auto* contentsRenderView = frameContentsRenderView(renderer);
        if (auto frameRootScrollingNodeID = contentsRenderView->frameView().scrollingNodeID()) {
            if (auto* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->insertNode(ScrollingNodeType::Subframe, frameRootScrollingNodeID, frameHostingNodeID, 0);
        }
    }

    // FIXME: Why always return true and not just when the layers changed?
    return true;
}

void RenderLayerCompositor::repaintCompositedLayers()
{
    recursiveRepaintLayer(rootRenderLayer());
}

void RenderLayerCompositor::recursiveRepaintLayer(RenderLayer& layer)
{
    layer.updateLayerListsIfNeeded();

    // FIXME: This method does not work correctly with transforms.
    if (layer.isComposited() && !layer.backing()->paintsIntoCompositedAncestor())
        layer.setBackingNeedsRepaint();

#if ASSERT_ENABLED
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

bool RenderLayerCompositor::layerRepaintTargetsBackingSharingLayer(RenderLayer& layer, BackingSharingState& sharingState) const
{
    if (!sharingState.backingProviderCandidate())
        return false;

    for (const RenderLayer* currLayer = &layer; currLayer; currLayer = currLayer->paintOrderParent()) {
        if (compositedWithOwnBackingStore(*currLayer))
            return false;
        
        if (currLayer->paintsIntoProvidedBacking())
            return false;

        if (sharingState.isPotentialBackingSharingLayer(*currLayer))
            return true;
    }

    return false;
}

RenderLayer& RenderLayerCompositor::rootRenderLayer() const
{
    return *m_renderView.layer();
}

GraphicsLayer* RenderLayerCompositor::rootGraphicsLayer() const
{
    if (m_overflowControlsHostLayer)
        return m_overflowControlsHostLayer.get();
    return m_rootContentsLayer.get();
}

void RenderLayerCompositor::setIsInWindow(bool isInWindow)
{
    LOG(Compositing, "RenderLayerCompositor %p setIsInWindow %d", this, isInWindow);

    if (!usesCompositing())
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
        if (m_legacyScrollingLayerCoordinator) {
            m_legacyScrollingLayerCoordinator->registerAllViewportConstrainedLayers(*this);
            m_legacyScrollingLayerCoordinator->registerAllScrollingLayers();
        }
#endif
    } else {
        if (m_rootLayerAttachment == RootLayerUnattached)
            return;

        detachRootLayer();
#if PLATFORM(IOS_FAMILY)
        if (m_legacyScrollingLayerCoordinator) {
            m_legacyScrollingLayerCoordinator->unregisterAllViewportConstrainedLayers();
            m_legacyScrollingLayerCoordinator->unregisterAllScrollingLayers();
        }
#endif
    }
}

void RenderLayerCompositor::invalidateEventRegionForAllFrames()
{
    for (AbstractFrame* frame = &page().mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (auto* view = localFrame->contentRenderer())
            view->compositor().invalidateEventRegionForAllLayers();
    }
}

void RenderLayerCompositor::invalidateEventRegionForAllLayers()
{
    applyToCompositedLayerIncludingDescendants(*m_renderView.layer(), [](auto& layer) {
        layer.invalidateEventRegion(RenderLayer::EventRegionInvalidationReason::SettingDidChange);
    });
}

void RenderLayerCompositor::clearBackingForAllLayers()
{
    applyToCompositedLayerIncludingDescendants(*m_renderView.layer(), [](auto& layer) { layer.clearBacking(); });
}

void RenderLayerCompositor::updateRootLayerPosition()
{
    if (m_rootContentsLayer) {
        m_rootContentsLayer->setSize(m_renderView.frameView().contentsSize());
        m_rootContentsLayer->setPosition(m_renderView.frameView().positionForRootContentLayer());
        m_rootContentsLayer->setAnchorPoint(FloatPoint3D());
    }

    updateScrollLayerClipping();

#if HAVE(RUBBER_BANDING)
    if (m_contentShadowLayer && m_rootContentsLayer) {
        m_contentShadowLayer->setPosition(m_rootContentsLayer->position());
        m_contentShadowLayer->setSize(m_rootContentsLayer->size());
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

bool RenderLayerCompositor::needsToBeComposited(const RenderLayer& layer, RequiresCompositingData& queryData) const
{
    if (!canBeComposited(layer))
        return false;

    return requiresCompositingLayer(layer, queryData) || layer.mustCompositeForIndirectReasons() || (usesCompositing() && layer.isRenderViewLayer());
}

// Note: this specifies whether the RL needs a compositing layer for intrinsic reasons.
// Use needsToBeComposited() to determine if a RL actually needs a compositing layer.
// FIXME: is clipsCompositingDescendants() an intrinsic reason?
bool RenderLayerCompositor::requiresCompositingLayer(const RenderLayer& layer, RequiresCompositingData& queryData) const
{
    auto& renderer = rendererForCompositingTests(layer);

    if (!renderer.layer()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    // The root layer always has a compositing layer, but it may not have backing.
    return requiresCompositingForTransform(renderer)
        || requiresCompositingForAnimation(renderer)
        || requiresCompositingForPosition(renderer, *renderer.layer(), queryData)
        || requiresCompositingForCanvas(renderer)
        || requiresCompositingForFilters(renderer)
        || requiresCompositingForWillChange(renderer)
        || requiresCompositingForBackfaceVisibility(renderer)
        || requiresCompositingForVideo(renderer)
        || requiresCompositingForModel(renderer)
        || requiresCompositingForFrame(renderer, queryData)
        || requiresCompositingForPlugin(renderer, queryData)
        || requiresCompositingForOverflowScrolling(*renderer.layer(), queryData);
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
    auto* fullScreenElement = document.fullscreenManager().fullscreenElement();

    if (!document.fullscreenManager().isFullscreen() || !fullScreenElement)
        return FullScreenDescendant::NotApplicable;

    auto* fullScreenRenderer = dynamicDowncast<RenderLayerModelObject>(fullScreenElement->renderer());
    if (!fullScreenRenderer)
        return FullScreenDescendant::NotApplicable;

    auto* fullScreenLayer = fullScreenRenderer->layer();
    if (!fullScreenLayer)
        return FullScreenDescendant::NotApplicable;

    auto backdropRenderer = fullScreenRenderer->backdropRenderer();
    if (backdropRenderer && backdropRenderer.get() == &layer.renderer())
        return FullScreenDescendant::Yes;

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

    RequiresCompositingData queryData;
    if (layer.isRenderViewLayer()
        || layer.transform() // note: excludes perspective and transformStyle3D.
        || requiresCompositingForAnimation(renderer)
        || requiresCompositingForPosition(renderer, layer, queryData)
        || requiresCompositingForCanvas(renderer)
        || requiresCompositingForFilters(renderer)
        || requiresCompositingForWillChange(renderer)
        || requiresCompositingForBackfaceVisibility(renderer)
        || requiresCompositingForVideo(renderer)
        || requiresCompositingForModel(renderer)
        || requiresCompositingForFrame(renderer, queryData)
        || requiresCompositingForPlugin(renderer, queryData)
        || requiresCompositingForOverflowScrolling(layer, queryData)
        || needsContentsCompositingLayer(layer)
        || renderer.isTransparent()
        || renderer.hasMask()
        || renderer.hasReflection()
        || renderer.hasFilter()
        || renderer.hasBackdropFilter())
        return true;

    if (layer.isComposited() && layer.backing()->hasBackingSharingLayers())
        return true;

    // FIXME: We really need to keep track of the ancestor layer that has its own backing store.
    if (!ancestorCompositedBounds.contains(layerCompositedBoundsInAncestor))
        return true;

    if (layer.mustCompositeForIndirectReasons()) {
        IndirectCompositingReason reason = layer.indirectCompositingReason();
        return reason == IndirectCompositingReason::Overlap
            || reason == IndirectCompositingReason::OverflowScrollPositioning
            || reason == IndirectCompositingReason::Stacking
            || reason == IndirectCompositingReason::BackgroundLayer
            || reason == IndirectCompositingReason::GraphicalEffect
            || reason == IndirectCompositingReason::Preserve3D; // preserve-3d has to create backing store to ensure that 3d-transformed elements intersect.
    }

    return false;
}

OptionSet<CompositingReason> RenderLayerCompositor::reasonsForCompositing(const RenderLayer& layer) const
{
    OptionSet<CompositingReason> reasons;

    if (!layer.isComposited())
        return reasons;

    RequiresCompositingData queryData;

    auto& renderer = rendererForCompositingTests(layer);

    if (requiresCompositingForTransform(renderer))
        reasons.add(CompositingReason::Transform3D);

    if (requiresCompositingForVideo(renderer))
        reasons.add(CompositingReason::Video);
    else if (requiresCompositingForCanvas(renderer))
        reasons.add(CompositingReason::Canvas);
    else if (requiresCompositingForModel(renderer))
        reasons.add(CompositingReason::Model);
    else if (requiresCompositingForPlugin(renderer, queryData))
        reasons.add(CompositingReason::Plugin);
    else if (requiresCompositingForFrame(renderer, queryData))
        reasons.add(CompositingReason::IFrame);

    if ((canRender3DTransforms() && renderer.style().backfaceVisibility() == BackfaceVisibility::Hidden))
        reasons.add(CompositingReason::BackfaceVisibilityHidden);

    if (requiresCompositingForAnimation(renderer))
        reasons.add(CompositingReason::Animation);

    if (requiresCompositingForFilters(renderer))
        reasons.add(CompositingReason::Filters);

    if (requiresCompositingForWillChange(renderer))
        reasons.add(CompositingReason::WillChange);

    if (requiresCompositingForPosition(renderer, *renderer.layer(), queryData))
        reasons.add(renderer.isFixedPositioned() ? CompositingReason::PositionFixed : CompositingReason::PositionSticky);

    if (requiresCompositingForOverflowScrolling(*renderer.layer(), queryData))
        reasons.add(CompositingReason::OverflowScrolling);

    switch (renderer.layer()->indirectCompositingReason()) {
    case IndirectCompositingReason::None:
        break;
    case IndirectCompositingReason::Clipping:
        reasons.add(CompositingReason::ClipsCompositingDescendants);
        break;
    case IndirectCompositingReason::Stacking:
        reasons.add(CompositingReason::Stacking);
        break;
    case IndirectCompositingReason::OverflowScrollPositioning:
        reasons.add(CompositingReason::OverflowScrollPositioning);
        break;
    case IndirectCompositingReason::Overlap:
        reasons.add(CompositingReason::Overlap);
        break;
    case IndirectCompositingReason::BackgroundLayer:
        reasons.add(CompositingReason::NegativeZIndexChildren);
        break;
    case IndirectCompositingReason::GraphicalEffect:
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
        if (renderer.hasClipPath())
            reasons.add(CompositingReason::ClipsCompositingDescendants);
        break;
    case IndirectCompositingReason::Perspective:
        reasons.add(CompositingReason::Perspective);
        break;
    case IndirectCompositingReason::Preserve3D:
        reasons.add(CompositingReason::Preserve3D);
        break;
    }

    if (usesCompositing() && renderer.layer()->isRenderViewLayer())
        reasons.add(CompositingReason::Root);

    return reasons;
}

static const char* compositingReasonToString(CompositingReason reason)
{
    switch (reason) {
    case CompositingReason::Transform3D: return "3D transform";
    case CompositingReason::Video: return "video";
    case CompositingReason::Canvas: return "canvas";
    case CompositingReason::Plugin: return "plugin";
    case CompositingReason::IFrame: return "iframe";
    case CompositingReason::BackfaceVisibilityHidden: return "backface-visibility: hidden";
    case CompositingReason::ClipsCompositingDescendants: return "clips compositing descendants";
    case CompositingReason::Animation: return "animation";
    case CompositingReason::Filters: return "filters";
    case CompositingReason::PositionFixed: return "position: fixed";
    case CompositingReason::PositionSticky: return "position: sticky";
    case CompositingReason::OverflowScrolling: return "async overflow scrolling";
    case CompositingReason::Stacking: return "stacking";
    case CompositingReason::Overlap: return "overlap";
    case CompositingReason::OverflowScrollPositioning: return "overflow scroll positioning";
    case CompositingReason::NegativeZIndexChildren: return "negative z-index children";
    case CompositingReason::TransformWithCompositedDescendants: return "transform with composited descendants";
    case CompositingReason::OpacityWithCompositedDescendants: return "opacity with composited descendants";
    case CompositingReason::MaskWithCompositedDescendants: return "mask with composited descendants";
    case CompositingReason::ReflectionWithCompositedDescendants: return "reflection with composited descendants";
    case CompositingReason::FilterWithCompositedDescendants: return "filter with composited descendants";
    case CompositingReason::BlendingWithCompositedDescendants: return "blending with composited descendants";
    case CompositingReason::IsolatesCompositedBlendingDescendants: return "isolates composited blending descendants";
    case CompositingReason::Perspective: return "perspective";
    case CompositingReason::Preserve3D: return "preserve-3d";
    case CompositingReason::WillChange: return "will-change";
    case CompositingReason::Root: return "root";
    case CompositingReason::Model: return "model";
    }
    return "";
}

#if !LOG_DISABLED
const char* RenderLayerCompositor::logOneReasonForCompositing(const RenderLayer& layer)
{
    for (auto reason : reasonsForCompositing(layer))
        return compositingReasonToString(reason);
    return "";
}
#endif


static bool canUseDescendantClippingLayer(const RenderLayer& layer)
{
    if (layer.isolatesCompositedBlending())
        return false;

    // We can only use the "descendant clipping layer" strategy when the clip rect is entirely within
    // the border box, because of interactions with border-radius clipping and compositing.
    if (auto* renderer = layer.renderBox(); renderer && renderer->hasClip()) {
        auto borderBoxRect = renderer->borderBoxRect();
        auto clipRect = renderer->clipRect({ }, nullptr);
        
        bool clipRectInsideBorderRect = intersection(borderBoxRect, clipRect) == clipRect;
        return clipRectInsideBorderRect;
    }

    return true;
}

// Return true if the given layer has some ancestor in the RenderLayer hierarchy that clips,
// up to the enclosing compositing ancestor. This is required because compositing layers are parented
// according to the z-order hierarchy, yet clipping goes down the renderer hierarchy.
// Thus, a RenderLayer can be clipped by a RenderLayer that is an ancestor in the renderer hierarchy,
// but a sibling in the z-order hierarchy.
// FIXME: can we do this without a tree walk?
bool RenderLayerCompositor::clippedByAncestor(RenderLayer& layer, const RenderLayer* compositingAncestor) const
{
    ASSERT(layer.isComposited());
    if (!compositingAncestor)
        return false;

    // If the compositingAncestor clips, that will be taken care of by clipsCompositingDescendants(),
    // so we only care about clipping between its first child that is our ancestor (the computeClipRoot),
    // and layer. The exception is when the compositingAncestor isolates composited blending children,
    // in this case it is not allowed to clipsCompositingDescendants() and each of its children
    // will be clippedByAncestor()s, including the compositingAncestor.
    auto* computeClipRoot = compositingAncestor;
    if (canUseDescendantClippingLayer(*compositingAncestor)) {
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

    auto backgroundClipRect = layer.backgroundClipRect(RenderLayer::ClipRectsContext(computeClipRoot, TemporaryClipRects));
    return !backgroundClipRect.isInfinite(); // FIXME: Incorrect for CSS regions.
}

bool RenderLayerCompositor::updateAncestorClippingStack(const RenderLayer& layer, const RenderLayer* compositingAncestor) const
{
    ASSERT(layer.isComposited());

    auto clippingStack = computeAncestorClippingStack(layer, compositingAncestor);
    return layer.backing()->updateAncestorClippingStack(WTFMove(clippingStack));
}

Vector<CompositedClipData> RenderLayerCompositor::computeAncestorClippingStack(const RenderLayer& layer, const RenderLayer* compositingAncestor) const
{
    // On first pass in WK1, the root may not have become composited yet.
    if (!compositingAncestor)
        return { };

    // We'll start by building a child-to-ancestors stack.
    Vector<CompositedClipData> newStack;

    // Walk up the containing block chain to composited ancestor, prepending an entry to the clip stack for:
    // * each composited scrolling layer
    // * each set of RenderLayers which contribute a clip.
    bool haveNonScrollableClippingIntermediateLayer = false;
    const RenderLayer* currentClippedLayer = &layer;
    
    auto pushNonScrollableClip = [&](const RenderLayer& clippedLayer, const RenderLayer& clippingRoot, ShouldRespectOverflowClip respectClip = IgnoreOverflowClip) {
        // Use IgnoreOverflowClip to ignore overflow contributed by clippingRoot (which may be a scroller).
        OptionSet<RenderLayer::ClipRectsOption> options;
        if (respectClip == RespectOverflowClip)
            options.add(RenderLayer::ClipRectsOption::RespectOverflowClip);

        auto backgroundClip = clippedLayer.backgroundClipRect(RenderLayer::ClipRectsContext(&clippingRoot, TemporaryClipRects, options));
        ASSERT(!backgroundClip.affectedByRadius());
        auto clipRect = backgroundClip.rect();
        if (clipRect.isInfinite())
            return;

        auto infiniteRect = LayoutRect::infiniteRect();
        auto renderableInfiniteRect = [] {
            // Return a infinite-like rect whose values are such that, when converted to float pixel values, they can reasonably represent device pixels.
            return LayoutRect(LayoutUnit::nearlyMin() / 32, LayoutUnit::nearlyMin() / 32, LayoutUnit::nearlyMax() / 16, LayoutUnit::nearlyMax() / 16);
        }();

        if (clipRect.width() == infiniteRect.width()) {
            clipRect.setX(renderableInfiniteRect.x());
            clipRect.setWidth(renderableInfiniteRect.width());
        }

        if (clipRect.height() == infiniteRect.height()) {
            clipRect.setY(renderableInfiniteRect.y());
            clipRect.setHeight(renderableInfiniteRect.height());
        }

        auto offset = layer.convertToLayerCoords(&clippingRoot, { }, RenderLayer::AdjustForColumns);
        clipRect.moveBy(-offset);

        CompositedClipData clipData { const_cast<RenderLayer*>(&clippedLayer), RoundedRect { clipRect }, false };
        newStack.insert(0, WTFMove(clipData));
    };

    traverseAncestorLayers(layer, [&](const RenderLayer& ancestorLayer, bool isContainingBlockChain, bool /*isPaintOrderAncestor*/) {
        if (&ancestorLayer == compositingAncestor) {
            bool canUseDescendantClip = canUseDescendantClippingLayer(ancestorLayer);
            if (haveNonScrollableClippingIntermediateLayer)
                pushNonScrollableClip(*currentClippedLayer, ancestorLayer, !canUseDescendantClip ? RespectOverflowClip : IgnoreOverflowClip);
            else if (!canUseDescendantClip && newStack.isEmpty())
                pushNonScrollableClip(*currentClippedLayer, ancestorLayer, RespectOverflowClip);

            return AncestorTraversal::Stop;
        }

        if (isContainingBlockChain && ancestorLayer.renderer().hasClipOrNonVisibleOverflow()) {
            auto* box = ancestorLayer.renderBox();
            if (!box)
                return AncestorTraversal::Continue;

            if (ancestorLayer.hasCompositedScrollableOverflow()) {
                if (haveNonScrollableClippingIntermediateLayer) {
                    pushNonScrollableClip(*currentClippedLayer, ancestorLayer);
                    haveNonScrollableClippingIntermediateLayer = false;
                }

                auto clipRoundedRect = parentRelativeScrollableRect(ancestorLayer, &ancestorLayer);
                auto offset = layer.convertToLayerCoords(&ancestorLayer, { }, RenderLayer::AdjustForColumns);
                clipRoundedRect.moveBy(-offset);

                CompositedClipData clipData { const_cast<RenderLayer*>(&ancestorLayer), clipRoundedRect, true };
                newStack.insert(0, WTFMove(clipData));
                currentClippedLayer = &ancestorLayer;
            } else if (box->hasNonVisibleOverflow() && box->style().hasBorderRadius()) {
                if (haveNonScrollableClippingIntermediateLayer) {
                    pushNonScrollableClip(*currentClippedLayer, ancestorLayer);
                    haveNonScrollableClippingIntermediateLayer = false;
                }
                
                auto clipRoundedRect = box->style().getRoundedInnerBorderFor(box->borderBoxRect());

                auto offset = layer.convertToLayerCoords(&ancestorLayer, { }, RenderLayer::AdjustForColumns);
                auto rect = clipRoundedRect.rect();
                rect.moveBy(-offset);
                clipRoundedRect.setRect(rect);

                CompositedClipData clipData { const_cast<RenderLayer*>(&ancestorLayer), clipRoundedRect, false };
                newStack.insert(0, WTFMove(clipData));
                currentClippedLayer = &ancestorLayer;
            } else
                haveNonScrollableClippingIntermediateLayer = true;
        }

        return AncestorTraversal::Continue;
    });
    
    return newStack;
}

// Note that this returns the ScrollingNodeID of the scroller this layer is embedded in, not the layer's own ScrollingNodeID if it has one.
ScrollingNodeID RenderLayerCompositor::asyncScrollableContainerNodeID(const RenderObject& renderer)
{
    auto* enclosingLayer = renderer.enclosingLayer();
    if (!enclosingLayer)
        return 0;
    
    auto layerScrollingNodeID = [](const RenderLayer& layer) -> ScrollingNodeID {
        if (layer.isComposited())
            return layer.backing()->scrollingNodeIDForRole(ScrollCoordinationRole::Scrolling);
        return 0;
    };

    // If the renderer is inside the layer, we care about the layer's scrollability. Otherwise, we let traverseAncestorLayers look at ancestors.
    if (!renderer.hasLayer()) {
        if (auto scrollingNodeID = layerScrollingNodeID(*enclosingLayer))
            return scrollingNodeID;
    }

    ScrollingNodeID containerScrollingNodeID = 0;
    traverseAncestorLayers(*enclosingLayer, [&](const RenderLayer& ancestorLayer, bool isContainingBlockChain, bool /*isPaintOrderAncestor*/) {
        if (isContainingBlockChain && ancestorLayer.hasCompositedScrollableOverflow()) {
            containerScrollingNodeID = layerScrollingNodeID(ancestorLayer);
            return AncestorTraversal::Stop;
        }
        return AncestorTraversal::Continue;
    });

    return containerScrollingNodeID;
}

bool RenderLayerCompositor::isCompositedSubframeRenderer(const RenderObject& renderer)
{
    if (!is<RenderWidget>(renderer))
        return false;

    return downcast<RenderWidget>(renderer).requiresAcceleratedCompositing();
}

// Return true if the given layer is a stacking context and has compositing child
// layers that it needs to clip. In this case we insert a clipping GraphicsLayer
// into the hierarchy between this layer and its children in the z-order hierarchy.
bool RenderLayerCompositor::clipsCompositingDescendants(const RenderLayer& layer)
{
    if (!(layer.hasCompositingDescendant() && layer.renderer().hasClipOrNonVisibleOverflow()))
        return false;

    if (layer.hasCompositedNonContainedDescendants())
        return false;

    return canUseDescendantClippingLayer(layer);
}

bool RenderLayerCompositor::requiresCompositingForAnimation(RenderLayerModelObject& renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::AnimationTrigger))
        return false;

    if (auto styleable = Styleable::fromRenderer(renderer)) {
        if (auto* effectsStack = styleable->keyframeEffectStack()) {
            return (effectsStack->isCurrentlyAffectingProperty(CSSPropertyOpacity)
                && (usesCompositing() || (m_compositingTriggers & ChromeClient::AnimatedOpacityTrigger)))
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyFilter)
#if ENABLE(FILTERS_LEVEL_2)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyWebkitBackdropFilter)
#endif
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyTranslate)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyScale)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyRotate)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyTransform)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyOffsetAnchor)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyOffsetDistance)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyOffsetPath)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyOffsetPosition)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyOffsetRotate);
        }
    }

    return false;
}

static bool styleHas3DTransformOperation(const RenderStyle& style)
{
    return style.transform().has3DOperation()
        || (style.translate() && style.translate()->is3DOperation())
        || (style.scale() && style.scale()->is3DOperation())
        || (style.rotate() && style.rotate()->is3DOperation());
}

static bool styleTransformOperationsAreRepresentableIn2D(const RenderStyle& style)
{
    return style.transform().isRepresentableIn2D()
        && (!style.translate() || style.translate()->isRepresentableIn2D())
        && (!style.scale() || style.scale()->isRepresentableIn2D())
        && (!style.rotate() || style.rotate()->isRepresentableIn2D());
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
        return styleHas3DTransformOperation(renderer.style());
    case CompositingPolicy::Conservative:
        // Continue to allow pages to avoid the very slow software filter path.
        if (styleHas3DTransformOperation(renderer.style()) && renderer.hasFilter())
            return true;
        return styleTransformOperationsAreRepresentableIn2D(renderer.style()) ? false : true;
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
    if (stackingContext && stackingContext->renderer().style().preserves3D())
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
    if ((video.requiresImmediateCompositing() || video.shouldDisplayVideo()) && canAccelerateVideoRendering(video))
        return true;
#else
    UNUSED_PARAM(renderer);
#endif
    return false;
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
    isCanvasLargeEnoughToForceCompositing = !canvasArea.hasOverflowed() && canvasArea >= canvasAreaThresholdRequiringCompositing;
#endif

    CanvasCompositingStrategy compositingStrategy = canvasCompositingStrategy(renderer);
    if (compositingStrategy == CanvasAsLayerContents)
        return true;

    if (m_compositingPolicy == CompositingPolicy::Normal)
        return compositingStrategy == CanvasPaintedToLayer && isCanvasLargeEnoughToForceCompositing;

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
    // FIXME: does this require layout?
    if (renderer.layer() && isDescendantOfFullScreenLayer(*renderer.layer()) == FullScreenDescendant::No)
        return false;
#endif

#if !PLATFORM(MAC)
    // Ugly workaround for rdar://71881767. Undo when webkit.org/b/222092 and webkit.org/b/222132 are fixed.
    if (m_compositingPolicy == CompositingPolicy::Conservative)
        return false;
#endif

    if (is<RenderBox>(renderer))
        return true;

    return renderer.style().willChange()->canTriggerCompositingOnInline();
}

bool RenderLayerCompositor::requiresCompositingForModel(RenderLayerModelObject& renderer) const
{
#if ENABLE(MODEL_ELEMENT)
    if (is<RenderModel>(renderer))
        return true;
#else
    UNUSED_PARAM(renderer);
#endif

    return false;
}

bool RenderLayerCompositor::requiresCompositingForPlugin(RenderLayerModelObject& renderer, RequiresCompositingData& queryData) const
{
    if (!(m_compositingTriggers & ChromeClient::PluginTrigger))
        return false;

    bool isCompositedPlugin = is<RenderEmbeddedObject>(renderer) && downcast<RenderEmbeddedObject>(renderer).allowsAcceleratedCompositing();
    if (!isCompositedPlugin)
        return false;

    auto& pluginRenderer = downcast<RenderWidget>(renderer);
    if (pluginRenderer.style().visibility() != Visibility::Visible)
        return false;

    // If we can't reliably know the size of the plugin yet, don't change compositing state.
    if (queryData.layoutUpToDate == LayoutUpToDate::No) {
        queryData.reevaluateAfterLayout = true;
        return pluginRenderer.isComposited();
    }

    // Don't go into compositing mode if height or width are zero, or size is 1x1.
    IntRect contentBox = snappedIntRect(pluginRenderer.contentBoxRect());
    return (contentBox.height() * contentBox.width() > 1);
}
    
bool RenderLayerCompositor::requiresCompositingForFrame(RenderLayerModelObject& renderer, RequiresCompositingData& queryData) const
{
    if (!is<RenderWidget>(renderer))
        return false;

    auto& frameRenderer = downcast<RenderWidget>(renderer);
    if (frameRenderer.style().visibility() != Visibility::Visible)
        return false;

    if (!frameRenderer.requiresAcceleratedCompositing())
        return false;

    if (queryData.layoutUpToDate == LayoutUpToDate::No) {
        queryData.reevaluateAfterLayout = true;
        return frameRenderer.isComposited();
    }

    // Don't go into compositing mode if height or width are zero.
    return !snappedIntRect(frameRenderer.contentBoxRect()).isEmpty();
}

bool RenderLayerCompositor::requiresCompositingForScrollableFrame(RequiresCompositingData& queryData) const
{
    if (isMainFrameCompositor())
        return false;

#if PLATFORM(COCOA) || USE(NICOSIA)
    if (!m_renderView.settings().asyncFrameScrollingEnabled())
        return false;
#endif

    if (!(m_compositingTriggers & ChromeClient::ScrollableNonMainFrameTrigger))
        return false;

    if (queryData.layoutUpToDate == LayoutUpToDate::No) {
        queryData.reevaluateAfterLayout = true;
        return m_renderView.isComposited();
    }

    return m_renderView.frameView().isScrollable();
}

bool RenderLayerCompositor::requiresCompositingForPosition(RenderLayerModelObject& renderer, const RenderLayer& layer, RequiresCompositingData& queryData) const
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
    bool isFixed = renderer.isFixedPositioned();
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

    if (queryData.layoutUpToDate == LayoutUpToDate::No) {
        queryData.reevaluateAfterLayout = true;
        return layer.isComposited();
    }

    auto container = renderer.container();
    ASSERT(container);

    // Don't promote fixed position elements that are descendants of a non-view container, e.g. transformed elements.
    // They will stay fixed wrt the container rather than the enclosing frame.
    if (container != &m_renderView) {
        queryData.nonCompositedForPositionReason = RenderLayer::NotCompositedForNonViewContainer;
        return false;
    }

    bool paintsContent = layer.isVisuallyNonEmpty() || layer.hasVisibleDescendant();
    if (!paintsContent) {
        queryData.nonCompositedForPositionReason = RenderLayer::NotCompositedForNoVisibleContent;
        return false;
    }

    bool intersectsViewport = fixedLayerIntersectsViewport(layer);
    if (!intersectsViewport) {
        queryData.nonCompositedForPositionReason = RenderLayer::NotCompositedForBoundsOutOfView;
        LOG_WITH_STREAM(Compositing, stream << "Layer " << &layer << " is outside the viewport");
        return false;
    }

    return true;
}

bool RenderLayerCompositor::requiresCompositingForOverflowScrolling(const RenderLayer& layer, RequiresCompositingData& queryData) const
{
    if (!layer.canUseCompositedScrolling())
        return false;

    if (queryData.layoutUpToDate == LayoutUpToDate::No) {
        queryData.reevaluateAfterLayout = true;
        return layer.isComposited();
    }

    return layer.hasCompositedScrollableOverflow();
}

IndirectCompositingReason RenderLayerCompositor::computeIndirectCompositingReason(const RenderLayer& layer, bool hasCompositedDescendants, bool has3DTransformedDescendants, bool paintsIntoProvidedBacking) const
{
    // When a layer has composited descendants, some effects, like 2d transforms, filters, masks etc must be implemented
    // via compositing so that they also apply to those composited descendants.
    auto& renderer = layer.renderer();
    if (hasCompositedDescendants && (layer.isolatesCompositedBlending() || layer.transform() || renderer.createsGroup() || renderer.hasReflection()))
        return IndirectCompositingReason::GraphicalEffect;

    // A layer with preserve-3d or perspective only needs to be composited if there are descendant layers that
    // will be affected by the preserve-3d or perspective.
    if (has3DTransformedDescendants) {
        if (renderer.style().preserves3D())
            return IndirectCompositingReason::Preserve3D;
    
        if (renderer.style().hasPerspective())
            return IndirectCompositingReason::Perspective;
    }

    // If this layer scrolls independently from the layer that it would paint into, it needs to get composited.
    if (!paintsIntoProvidedBacking && layer.hasCompositedScrollingAncestor()) {
        auto* paintDestination = layer.paintOrderParent();
        if (paintDestination && layerScrollBehahaviorRelativeToCompositedAncestor(layer, *paintDestination) != ScrollPositioningBehavior::None)
            return IndirectCompositingReason::OverflowScrollPositioning;
    }

    // Check for clipping last; if compositing just for clipping, the layer doesn't need its own backing store.
    if (hasCompositedDescendants && clipsCompositingDescendants(layer))
        return IndirectCompositingReason::Clipping;

    return IndirectCompositingReason::None;
}

bool RenderLayerCompositor::styleChangeMayAffectIndirectCompositingReasons(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    if (RenderElement::createsGroupForStyle(newStyle) != RenderElement::createsGroupForStyle(oldStyle))
        return true;
    if (newStyle.isolation() != oldStyle.isolation())
        return true;
    if (newStyle.hasTransform() != oldStyle.hasTransform())
        return true;
    if (newStyle.boxReflect() != oldStyle.boxReflect())
        return true;
    if (newStyle.usedTransformStyle3D() != oldStyle.usedTransformStyle3D())
        return true;
    if (newStyle.hasPerspective() != oldStyle.hasPerspective())
        return true;

    return false;
}

bool RenderLayerCompositor::isAsyncScrollableStickyLayer(const RenderLayer& layer, const RenderLayer** enclosingAcceleratedOverflowLayer) const
{
    ASSERT(layer.renderer().isStickilyPositioned());

    auto* enclosingOverflowLayer = layer.enclosingOverflowClipLayer(ExcludeSelf);

    if (enclosingOverflowLayer && enclosingOverflowLayer->hasCompositedScrollableOverflow()) {
        if (enclosingAcceleratedOverflowLayer)
            *enclosingAcceleratedOverflowLayer = enclosingOverflowLayer;
        return true;
    }

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

    if (!(layer.renderer().isFixedPositioned() && layer.behavesAsFixed()))
        return false;

    for (auto* ancestor = layer.parent(); ancestor; ancestor = ancestor->parent()) {
        if (ancestor->hasCompositedScrollableOverflow())
            return true;
        if (ancestor->isStackingContext() && ancestor->isComposited() && ancestor->renderer().isFixedPositioned())
            return false;
    }

    return true;
}

bool RenderLayerCompositor::fixedLayerIntersectsViewport(const RenderLayer& layer) const
{
    ASSERT(layer.renderer().isFixedPositioned());

    // Fixed position elements that are invisible in the current view don't get their own layer.
    // FIXME: We shouldn't have to check useFixedLayout() here; one of the viewport rects needs to give the correct answer.
    LayoutRect viewBounds;
    if (m_renderView.frameView().useFixedLayout())
        viewBounds = m_renderView.unscaledDocumentRect();
    else
        viewBounds = m_renderView.frameView().rectForFixedPositionLayout();

    LayoutRect layerBounds = layer.calculateLayerBounds(&layer, LayoutSize(), { RenderLayer::UseLocalClipRectIfPossible, RenderLayer::IncludeFilterOutsets, RenderLayer::UseFragmentBoxesExcludingCompositing,
        RenderLayer::ExcludeHiddenDescendants, RenderLayer::DontConstrainForMask, RenderLayer::IncludeCompositedDescendants });
    // Map to m_renderView to ignore page scale.
    FloatRect absoluteBounds = layer.renderer().localToContainerQuad(FloatRect(layerBounds), &m_renderView).boundingBox();
    return viewBounds.intersects(enclosingIntRect(absoluteBounds));
}

bool RenderLayerCompositor::useCoordinatedScrollingForLayer(const RenderLayer& layer) const
{
    if (layer.isRenderViewLayer() && hasCoordinatedScrolling())
        return true;

    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->coordinatesScrollingForOverflowLayer(layer);

    return false;
}

ScrollPositioningBehavior RenderLayerCompositor::layerScrollBehahaviorRelativeToCompositedAncestor(const RenderLayer& layer, const RenderLayer& compositedAncestor)
{
    if (!layer.hasCompositedScrollingAncestor())
        return ScrollPositioningBehavior::None;

    auto needsMovesNode = [&] {
        bool result = false;
        traverseAncestorLayers(layer, [&](const RenderLayer& ancestorLayer, bool isContainingBlockChain, bool /* isPaintOrderAncestor */) {
            if (&ancestorLayer == &compositedAncestor)
                return AncestorTraversal::Stop;

            if (isContainingBlockChain && ancestorLayer.hasCompositedScrollableOverflow()) {
                result = true;
                return AncestorTraversal::Stop;
            }

            return AncestorTraversal::Continue;
        });

        return result;
    };

    if (needsMovesNode())
        return ScrollPositioningBehavior::Moves;

    if (layer.boxScrollingScope() != compositedAncestor.contentsScrollingScope())
        return ScrollPositioningBehavior::Stationary;

    return ScrollPositioningBehavior::None;
}

static void collectStationaryLayerRelatedOverflowNodes(const RenderLayer& layer, const RenderLayer&, Vector<ScrollingNodeID>& scrollingNodes)
{
    ASSERT(layer.isComposited());

    auto appendOverflowLayerNodeID = [&scrollingNodes] (const RenderLayer& overflowLayer) {
        ASSERT(overflowLayer.isComposited());
        auto scrollingNodeID = overflowLayer.backing()->scrollingNodeIDForRole(ScrollCoordinationRole::Scrolling);
        if (scrollingNodeID)
            scrollingNodes.append(scrollingNodeID);
        else
            LOG(Scrolling, "Layer %p doesn't have scrolling node ID yet", &overflowLayer);
    };

    // Collect all the composited scrollers which affect the position of this layer relative to its compositing ancestor (which might be inside the scroller or the scroller itself).
    bool seenPaintOrderAncestor = false;
    traverseAncestorLayers(layer, [&](const RenderLayer& ancestorLayer, bool isContainingBlockChain, bool isPaintOrderAncestor) {
        seenPaintOrderAncestor |= isPaintOrderAncestor;
        if (isContainingBlockChain && isPaintOrderAncestor)
            return AncestorTraversal::Stop;

        if (!ancestorLayer.isComposited())
            return AncestorTraversal::Stop;

        if (seenPaintOrderAncestor && !isContainingBlockChain && ancestorLayer.hasCompositedScrollableOverflow())
            appendOverflowLayerNodeID(ancestorLayer);

        return AncestorTraversal::Continue;
    });
}

ScrollPositioningBehavior RenderLayerCompositor::computeCoordinatedPositioningForLayer(const RenderLayer& layer, const RenderLayer* compositedAncestor) const
{
    if (layer.isRenderViewLayer())
        return ScrollPositioningBehavior::None;

    if (layer.renderer().isFixedPositioned())
        return ScrollPositioningBehavior::None;
    
    if (!layer.hasCompositedScrollingAncestor())
        return ScrollPositioningBehavior::None;

    auto* scrollingCoordinator = this->scrollingCoordinator();
    if (!scrollingCoordinator)
        return ScrollPositioningBehavior::None;

    if (!compositedAncestor) {
        ASSERT_NOT_REACHED();
        return ScrollPositioningBehavior::None;
    }

    return layerScrollBehahaviorRelativeToCompositedAncestor(layer, *compositedAncestor);
}

static Vector<ScrollingNodeID> collectRelatedCoordinatedScrollingNodes(const RenderLayer& layer, ScrollPositioningBehavior positioningBehavior)
{
    Vector<ScrollingNodeID> overflowNodeIDs;

    switch (positioningBehavior) {
    case ScrollPositioningBehavior::Stationary: {
        auto* compositedAncestor = layer.ancestorCompositingLayer();
        if (!compositedAncestor)
            return overflowNodeIDs;
        collectStationaryLayerRelatedOverflowNodes(layer, *compositedAncestor, overflowNodeIDs);
        break;
    }
    case ScrollPositioningBehavior::Moves:
    case ScrollPositioningBehavior::None:
        ASSERT_NOT_REACHED();
        break;
    }

    return overflowNodeIDs;
}

bool RenderLayerCompositor::isLayerForIFrameWithScrollCoordinatedContents(const RenderLayer& layer) const
{
    if (!is<RenderWidget>(layer.renderer()))
        return false;

    auto* contentDocument = downcast<RenderWidget>(layer.renderer()).frameOwnerElement().contentDocument();
    if (!contentDocument)
        return false;

    auto* view = contentDocument->renderView();
    if (!view)
        return false;

    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->coordinatesScrollingForFrameView(view->frameView());

    return false;
}

bool RenderLayerCompositor::isRunningTransformAnimation(RenderLayerModelObject& renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::AnimationTrigger))
        return false;

    if (auto styleable = Styleable::fromRenderer(renderer)) {
        if (auto* effectsStack = styleable->keyframeEffectStack())
            return effectsStack->isCurrentlyAffectingProperty(CSSPropertyTransform)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyRotate)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyScale)
                || effectsStack->isCurrentlyAffectingProperty(CSSPropertyTranslate);
    }

    return false;
}

// If an element has composited negative z-index children, those children render in front of the
// layer background, so we need an extra 'contents' layer for the foreground of the layer object.
bool RenderLayerCompositor::needsContentsCompositingLayer(const RenderLayer& layer) const
{
    for (auto* layer : layer.negativeZOrderLayers()) {
        if (layer->isComposited() || layer->hasCompositingDescendant())
            return true;
    }

    return false;
}

bool RenderLayerCompositor::requiresScrollLayer(RootLayerAttachment attachment) const
{
    auto& frameView = m_renderView.frameView();

    // This applies when the application UI handles scrolling, in which case RenderLayerCompositor doesn't need to manage it.
    if (frameView.delegatedScrollingMode() == DelegatedScrollingMode::DelegatedToNativeScrollView && isMainFrameCompositor())
        return false;

    // We need to handle our own scrolling if we're:
    return !m_renderView.frameView().platformWidget() // viewless (i.e. non-Mac, or Mac in WebKit2)
        || attachment == RootLayerAttachedViaEnclosingFrame; // a composited frame on Mac
}

void paintScrollbar(Scrollbar* scrollbar, GraphicsContext& context, const IntRect& clip, const Color& backgroundColor)
{
    if (!scrollbar)
        return;

    context.save();
    const IntRect& scrollbarRect = scrollbar->frameRect();
    context.translate(-scrollbarRect.location());
    IntRect transformedClip = clip;
    transformedClip.moveBy(scrollbarRect.location());
#if HAVE(RUBBER_BANDING)
    UNUSED_PARAM(backgroundColor);
#else
    if (!scrollbar->isOverlayScrollbar() && backgroundColor.isVisible())
        context.fillRect(transformedClip, backgroundColor);
#endif
    scrollbar->paint(context, transformedClip);
    context.restore();
}

void RenderLayerCompositor::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, const FloatRect& clip, GraphicsLayerPaintBehavior)
{
#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(m_renderView.useDarkAppearance());
#endif

    IntRect pixelSnappedRectForIntegralPositionedItems = snappedIntRect(LayoutRect(clip));
    if (graphicsLayer == layerForHorizontalScrollbar())
        paintScrollbar(m_renderView.frameView().horizontalScrollbar(), context, pixelSnappedRectForIntegralPositionedItems, m_viewBackgroundColor);
    else if (graphicsLayer == layerForVerticalScrollbar())
        paintScrollbar(m_renderView.frameView().verticalScrollbar(), context, pixelSnappedRectForIntegralPositionedItems, m_viewBackgroundColor);
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
    auto* localFrame = dynamicDowncast<LocalFrame>(m_renderView.frameView().frame());
    if (!localFrame)
        return false;
    return localFrame->isMainFrame();
}

bool RenderLayerCompositor::shouldCompositeOverflowControls() const
{
    auto& frameView = m_renderView.frameView();

    if (!frameView.managesScrollbars())
        return false;

    if (documentUsesTiledBacking())
        return true;

    if (m_overflowControlsHostLayer && isMainFrameCompositor())
        return true;

#if !USE(COORDINATED_GRAPHICS)
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

#if HAVE(RUBBER_BANDING)
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
        m_layerForTopOverhangArea->setName(MAKE_STATIC_STRING_IMPL("top overhang"));
        m_scrolledContentsLayer->addChildBelow(*m_layerForTopOverhangArea, m_rootContentsLayer.get());
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
        m_layerForBottomOverhangArea->setName(MAKE_STATIC_STRING_IMPL("bottom overhang"));
        m_scrolledContentsLayer->addChildBelow(*m_layerForBottomOverhangArea, m_rootContentsLayer.get());
    }

    m_layerForBottomOverhangArea->setPosition(FloatPoint(0, m_rootContentsLayer->size().height() + m_renderView.frameView().headerHeight()
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
        m_layerForHeader->setName(MAKE_STATIC_STRING_IMPL("header"));
        m_scrolledContentsLayer->addChildAbove(*m_layerForHeader, m_rootContentsLayer.get());
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
        m_layerForFooter->setName(MAKE_STATIC_STRING_IMPL("footer"));
        m_scrolledContentsLayer->addChildAbove(*m_layerForFooter, m_rootContentsLayer.get());
    }

    float totalContentHeight = m_rootContentsLayer->size().height() + m_renderView.frameView().headerHeight() + m_renderView.frameView().footerHeight();
    m_layerForFooter->setPosition(FloatPoint(0, FrameView::yPositionForFooterLayer(m_renderView.frameView().scrollPosition(),
        m_renderView.frameView().topContentInset(), totalContentHeight, m_renderView.frameView().footerHeight())));
    m_layerForFooter->setAnchorPoint(FloatPoint3D());
    m_layerForFooter->setSize(FloatSize(m_renderView.frameView().visibleWidth(), m_renderView.frameView().footerHeight()));

    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->frameViewRootLayerDidChange(m_renderView.frameView());

    page().chrome().client().didAddFooterLayer(*m_layerForFooter);

    return m_layerForFooter.get();
}

void RenderLayerCompositor::updateLayerForOverhangAreasBackgroundColor()
{
    if (!m_layerForOverhangAreas)
        return;

    Color backgroundColor;

    if (m_renderView.settings().backgroundShouldExtendBeyondPage()) {
        backgroundColor = ([&] {
            if (auto underPageBackgroundColorOverride = page().underPageBackgroundColorOverride(); underPageBackgroundColorOverride.isValid())
                return underPageBackgroundColorOverride;

            return m_rootExtendedBackgroundColor;
        })();
        m_layerForOverhangAreas->setBackgroundColor(backgroundColor);
    }

    if (!backgroundColor.isValid())
        m_layerForOverhangAreas->setCustomAppearance(GraphicsLayer::CustomAppearance::ScrollingOverhang);
}

#endif // HAVE(RUBBER_BANDING)

bool RenderLayerCompositor::viewNeedsToInvalidateEventRegionOfEnclosingCompositingLayerForRepaint() const
{
    // Event regions are only updated on compositing layers. Non-composited layers must
    // delegate to their enclosing compositing layer for repaint to update the event region
    // for elements inside them.
    return !m_renderView.isComposited();
}

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
    if (!usesCompositing())
        return;

    Color oldBackgroundColor;
    if (oldStyle)
        oldBackgroundColor = oldStyle->visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);

    if (oldBackgroundColor != renderer.style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor))
        rootBackgroundColorOrTransparencyChanged();

    bool hadFixedBackground = oldStyle && oldStyle->hasEntirelyFixedBackground();
    if (hadFixedBackground != renderer.style().hasEntirelyFixedBackground())
        rootLayerConfigurationChanged();
    
    if (oldStyle && (oldStyle->overscrollBehaviorX() != renderer.style().overscrollBehaviorX() || oldStyle->overscrollBehaviorY() != renderer.style().overscrollBehaviorY())) {
        if (auto* layer = m_renderView.layer())
            layer->setNeedsCompositingGeometryUpdate();
    }
}

void RenderLayerCompositor::rootBackgroundColorOrTransparencyChanged()
{
    if (!usesCompositing())
        return;

    Color backgroundColor;
    bool isTransparent = viewHasTransparentBackground(&backgroundColor);
    
    Color extendedBackgroundColor = m_renderView.settings().backgroundShouldExtendBeyondPage() ? backgroundColor : Color();
    
    bool transparencyChanged = m_viewBackgroundIsTransparent != isTransparent;
    bool backgroundColorChanged = m_viewBackgroundColor != backgroundColor;
    bool extendedBackgroundColorChanged = m_rootExtendedBackgroundColor != extendedBackgroundColor;

    if (!transparencyChanged && !backgroundColorChanged && !extendedBackgroundColorChanged)
        return;

    LOG(Compositing, "RenderLayerCompositor %p rootBackgroundColorOrTransparencyChanged. isTransparent=%d", this, isTransparent);

    m_viewBackgroundIsTransparent = isTransparent;
    m_viewBackgroundColor = backgroundColor;
    m_rootExtendedBackgroundColor = extendedBackgroundColor;
    
    if (extendedBackgroundColorChanged) {
        page().chrome().client().pageExtendedBackgroundColorDidChange();
        
#if HAVE(RUBBER_BANDING)
        updateLayerForOverhangAreasBackgroundColor();
#endif
    }
    
    rootLayerConfigurationChanged();
}

void RenderLayerCompositor::updateOverflowControlsLayers()
{
#if HAVE(RUBBER_BANDING)
    if (requiresOverhangAreasLayer()) {
        if (!m_layerForOverhangAreas) {
            m_layerForOverhangAreas = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_layerForOverhangAreas->setName(MAKE_STATIC_STRING_IMPL("overhang areas"));
            m_layerForOverhangAreas->setDrawsContent(false);

            float topContentInset = m_renderView.frameView().topContentInset();
            IntSize overhangAreaSize = m_renderView.frameView().frameRect().size();
            overhangAreaSize.setHeight(overhangAreaSize.height() - topContentInset);
            m_layerForOverhangAreas->setSize(overhangAreaSize);
            m_layerForOverhangAreas->setPosition(FloatPoint(0, topContentInset));
            m_layerForOverhangAreas->setAnchorPoint(FloatPoint3D());
            updateLayerForOverhangAreasBackgroundColor();

            // We want the overhang areas layer to be positioned below the frame contents,
            // so insert it below the clip layer.
            m_overflowControlsHostLayer->addChildBelow(*m_layerForOverhangAreas, layerForClipping());
        }
    } else
        GraphicsLayer::unparentAndClear(m_layerForOverhangAreas);

    if (requiresContentShadowLayer()) {
        if (!m_contentShadowLayer) {
            m_contentShadowLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_contentShadowLayer->setName(MAKE_STATIC_STRING_IMPL("content shadow"));
            m_contentShadowLayer->setSize(m_rootContentsLayer->size());
            m_contentShadowLayer->setPosition(m_rootContentsLayer->position());
            m_contentShadowLayer->setAnchorPoint(FloatPoint3D());
            m_contentShadowLayer->setCustomAppearance(GraphicsLayer::CustomAppearance::ScrollingShadow);

            m_scrolledContentsLayer->addChildBelow(*m_contentShadowLayer, m_rootContentsLayer.get());
        }
    } else
        GraphicsLayer::unparentAndClear(m_contentShadowLayer);
#endif

    if (requiresHorizontalScrollbarLayer()) {
        if (!m_layerForHorizontalScrollbar) {
            m_layerForHorizontalScrollbar = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_layerForHorizontalScrollbar->setAllowsBackingStoreDetaching(false);
            m_layerForHorizontalScrollbar->setAllowsTiling(false);
            m_layerForHorizontalScrollbar->setShowDebugBorder(m_showDebugBorders);
            m_layerForHorizontalScrollbar->setName(MAKE_STATIC_STRING_IMPL("horizontal scrollbar container"));
#if PLATFORM(COCOA) && USE(CA)
            m_layerForHorizontalScrollbar->setAcceleratesDrawing(acceleratedDrawingEnabled());
#endif
            m_overflowControlsHostLayer->addChild(*m_layerForHorizontalScrollbar);

            if (auto* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), ScrollbarOrientation::Horizontal);
        }
    } else if (m_layerForHorizontalScrollbar) {
        GraphicsLayer::unparentAndClear(m_layerForHorizontalScrollbar);

        if (auto* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), ScrollbarOrientation::Horizontal);
    }

    if (requiresVerticalScrollbarLayer()) {
        if (!m_layerForVerticalScrollbar) {
            m_layerForVerticalScrollbar = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_layerForVerticalScrollbar->setAllowsBackingStoreDetaching(false);
            m_layerForVerticalScrollbar->setAllowsTiling(false);
            m_layerForVerticalScrollbar->setShowDebugBorder(m_showDebugBorders);
            m_layerForVerticalScrollbar->setName(MAKE_STATIC_STRING_IMPL("vertical scrollbar container"));
#if PLATFORM(COCOA) && USE(CA)
            m_layerForVerticalScrollbar->setAcceleratesDrawing(acceleratedDrawingEnabled());
#endif
            m_overflowControlsHostLayer->addChild(*m_layerForVerticalScrollbar);

            if (auto* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), ScrollbarOrientation::Vertical);
        }
    } else if (m_layerForVerticalScrollbar) {
        GraphicsLayer::unparentAndClear(m_layerForVerticalScrollbar);

        if (auto* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), ScrollbarOrientation::Vertical);
    }

    if (requiresScrollCornerLayer()) {
        if (!m_layerForScrollCorner) {
            m_layerForScrollCorner = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_layerForScrollCorner->setAllowsBackingStoreDetaching(false);
            m_layerForScrollCorner->setShowDebugBorder(m_showDebugBorders);
            m_layerForScrollCorner->setName(MAKE_STATIC_STRING_IMPL("scroll corner"));
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

    if (!m_rootContentsLayer) {
        m_rootContentsLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
        m_rootContentsLayer->setName(MAKE_STATIC_STRING_IMPL("content root"));
        IntRect overflowRect = snappedIntRect(m_renderView.layoutOverflowRect());
        m_rootContentsLayer->setSize(FloatSize(overflowRect.maxX(), overflowRect.maxY()));
        m_rootContentsLayer->setPosition(FloatPoint());

#if PLATFORM(IOS_FAMILY)
        // Page scale is applied above this on iOS, so we'll just say that our root layer applies it.
        auto* frame = dynamicDowncast<LocalFrame>(m_renderView.frameView().frame());
        if (frame && frame->isMainFrame())
            m_rootContentsLayer->setAppliesPageScale();
#endif

        // Need to clip to prevent transformed content showing outside this frame
        updateRootContentLayerClipping();
    }

    if (requiresScrollLayer(expectedAttachment)) {
        if (!m_overflowControlsHostLayer) {
            ASSERT(!m_scrolledContentsLayer);
            ASSERT(!m_clipLayer);

            // Create a layer to host the clipping layer and the overflow controls layers.
            m_overflowControlsHostLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
            m_overflowControlsHostLayer->setName(MAKE_STATIC_STRING_IMPL("overflow controls host"));

            m_scrolledContentsLayer = GraphicsLayer::create(graphicsLayerFactory(), *this, GraphicsLayer::Type::ScrolledContents);
            m_scrolledContentsLayer->setName(MAKE_STATIC_STRING_IMPL("frame scrolled contents"));
            m_scrolledContentsLayer->setAnchorPoint({ });

#if PLATFORM(IOS_FAMILY)
            if (m_renderView.settings().asyncFrameScrollingEnabled()) {
                m_scrollContainerLayer = GraphicsLayer::create(graphicsLayerFactory(), *this, GraphicsLayer::Type::ScrollContainer);

                m_scrollContainerLayer->setName(MAKE_STATIC_STRING_IMPL("scroll container"));
                m_scrollContainerLayer->setMasksToBounds(true);
                m_scrollContainerLayer->setAnchorPoint({ });

                m_scrollContainerLayer->addChild(*m_scrolledContentsLayer);
                m_overflowControlsHostLayer->addChild(*m_scrollContainerLayer);
            }
#endif
            // FIXME: m_scrollContainerLayer and m_clipLayer have similar roles here, but m_clipLayer has some special positioning to
            // account for clipping and top content inset (see FrameView::yPositionForInsetClipLayer()).
            if (!m_scrollContainerLayer) {
                m_clipLayer = GraphicsLayer::create(graphicsLayerFactory(), *this);
                m_clipLayer->setName(MAKE_STATIC_STRING_IMPL("frame clipping"));
                m_clipLayer->setMasksToBounds(true);
                m_clipLayer->setAnchorPoint({ });

                m_clipLayer->addChild(*m_scrolledContentsLayer);
                m_overflowControlsHostLayer->addChild(*m_clipLayer);
            }

            m_scrolledContentsLayer->addChild(*m_rootContentsLayer);

            updateScrollLayerClipping();
            updateOverflowControlsLayers();

            if (hasCoordinatedScrolling())
                scheduleRenderingUpdate();
            else
                updateScrollLayerPosition();
        }
    } else {
        if (m_overflowControlsHostLayer) {
            GraphicsLayer::unparentAndClear(m_overflowControlsHostLayer);
            GraphicsLayer::unparentAndClear(m_clipLayer);
            GraphicsLayer::unparentAndClear(m_scrollContainerLayer);
            GraphicsLayer::unparentAndClear(m_scrolledContentsLayer);
        }
    }

    // Check to see if we have to change the attachment
    if (m_rootLayerAttachment != RootLayerUnattached)
        detachRootLayer();

    attachRootLayer(expectedAttachment);
}

void RenderLayerCompositor::destroyRootLayer()
{
    if (!m_rootContentsLayer)
        return;

    detachRootLayer();

#if HAVE(RUBBER_BANDING)
    GraphicsLayer::unparentAndClear(m_layerForOverhangAreas);
#endif

    if (m_layerForHorizontalScrollbar) {
        GraphicsLayer::unparentAndClear(m_layerForHorizontalScrollbar);
        if (auto* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), ScrollbarOrientation::Horizontal);
        if (auto* horizontalScrollbar = m_renderView.frameView().horizontalScrollbar())
            m_renderView.frameView().invalidateScrollbar(*horizontalScrollbar, IntRect(IntPoint(0, 0), horizontalScrollbar->frameRect().size()));
    }

    if (m_layerForVerticalScrollbar) {
        GraphicsLayer::unparentAndClear(m_layerForVerticalScrollbar);
        if (auto* scrollingCoordinator = this->scrollingCoordinator())
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_renderView.frameView(), ScrollbarOrientation::Vertical);
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
        GraphicsLayer::unparentAndClear(m_scrollContainerLayer);
        GraphicsLayer::unparentAndClear(m_scrolledContentsLayer);
    }
    ASSERT(!m_scrolledContentsLayer);
    GraphicsLayer::unparentAndClear(m_rootContentsLayer);

    m_layerUpdater = nullptr;
}

void RenderLayerCompositor::attachRootLayer(RootLayerAttachment attachment)
{
    if (!m_rootContentsLayer)
        return;

    LOG(Compositing, "RenderLayerCompositor %p attachRootLayer %d", this, attachment);

    switch (attachment) {
        case RootLayerUnattached:
            ASSERT_NOT_REACHED();
            break;
        case RootLayerAttachedViaChromeClient: {
            if (auto* frame = dynamicDowncast<LocalFrame>(m_renderView.frameView().frame()))
                page().chrome().client().attachRootGraphicsLayer(*frame, rootGraphicsLayer());
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
        scheduleRenderingUpdate();
        m_shouldFlushOnReattach = false;
    }
}

void RenderLayerCompositor::detachRootLayer()
{
    if (!m_rootContentsLayer || m_rootLayerAttachment == RootLayerUnattached)
        return;

    if (auto* scrollingCoordinator = this->scrollingCoordinator())
        scrollingCoordinator->frameViewWillBeDetached(m_renderView.frameView());

    switch (m_rootLayerAttachment) {
    case RootLayerAttachedViaEnclosingFrame: {
        // The layer will get unhooked up via RenderLayerBacking::updateConfiguration()
        // for the frame's renderer in the parent document.
        if (m_overflowControlsHostLayer)
            m_overflowControlsHostLayer->removeFromParent();
        else
            m_rootContentsLayer->removeFromParent();

        if (auto* ownerElement = m_renderView.document().ownerElement())
            ownerElement->scheduleInvalidateStyleAndLayerComposition();

        if (auto frameRootScrollingNodeID = m_renderView.frameView().scrollingNodeID()) {
            if (auto* scrollingCoordinator = this->scrollingCoordinator()) {
                scrollingCoordinator->frameViewWillBeDetached(m_renderView.frameView());
                scrollingCoordinator->unparentNode(frameRootScrollingNodeID);
            }
        }
        break;
    }
    case RootLayerAttachedViaChromeClient: {
        if (auto* frame = dynamicDowncast<LocalFrame>(m_renderView.frameView().frame())) {
            if (auto* scrollingCoordinator = this->scrollingCoordinator())
                scrollingCoordinator->frameViewWillBeDetached(m_renderView.frameView());
            page().chrome().client().attachRootGraphicsLayer(*frame, nullptr);
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

    // The attachment can affect whether the RenderView layer's paintsIntoWindow() behavior,
    // so call updateDrawsContent() to update that.
    auto* layer = m_renderView.layer();
    if (auto* backing = layer ? layer->backing() : nullptr)
        backing->updateDrawsContent();

    if (auto* frame = dynamicDowncast<LocalFrame>(m_renderView.frameView().frame()); !frame || !frame->isMainFrame())
        return;

    Ref<GraphicsLayer> overlayHost = page().pageOverlayController().layerWithDocumentOverlays();
    m_rootContentsLayer->addChild(WTFMove(overlayHost));
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

    if (style.preserves3D() || style.hasPerspective() || styleHas3DTransformOperation(style))
        return true;

    const_cast<RenderLayer&>(layer).updateLayerListsIfNeeded();

#if ASSERT_ENABLED
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

void RenderLayerCompositor::removeFromScrollCoordinatedLayers(RenderLayer& layer)
{
#if PLATFORM(IOS_FAMILY)
    if (m_legacyScrollingLayerCoordinator)
        m_legacyScrollingLayerCoordinator->removeLayer(layer);
#endif

    detachScrollCoordinatedLayer(layer, allScrollCoordinationRoles());
}

FixedPositionViewportConstraints RenderLayerCompositor::computeFixedViewportConstraints(RenderLayer& layer) const
{
    ASSERT(layer.isComposited());

    auto* anchorLayer = layer.backing()->viewportAnchorLayer();
    if (!anchorLayer) {
        ASSERT_NOT_REACHED();
        return { };
    }

    FixedPositionViewportConstraints constraints;
    constraints.setLayerPositionAtLastLayout(anchorLayer->position());
    constraints.setViewportRectAtLastLayout(m_renderView.frameView().rectForFixedPositionLayout());
    constraints.setAlignmentOffset(anchorLayer->pixelAlignmentOffset());

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

    auto& renderer = downcast<RenderBoxModelObject>(layer.renderer());

    auto* anchorLayer = layer.backing()->viewportAnchorLayer();
    if (!anchorLayer) {
        ASSERT_NOT_REACHED();
        return { };
    }

    StickyPositionViewportConstraints constraints;
    renderer.computeStickyPositionConstraints(constraints, renderer.constrainingRectForStickyPosition());

    constraints.setLayerPositionAtLastLayout(anchorLayer->position());
    constraints.setStickyOffsetAtLastLayout(renderer.stickyPositionOffset());
    constraints.setAlignmentOffset(anchorLayer->pixelAlignmentOffset());

    return constraints;
}

static inline ScrollCoordinationRole scrollCoordinationRoleForNodeType(ScrollingNodeType nodeType)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
    case ScrollingNodeType::Overflow:
        return ScrollCoordinationRole::Scrolling;
    case ScrollingNodeType::OverflowProxy:
        return ScrollCoordinationRole::ScrollingProxy;
    case ScrollingNodeType::FrameHosting:
        return ScrollCoordinationRole::FrameHosting;
    case ScrollingNodeType::Fixed:
    case ScrollingNodeType::Sticky:
        return ScrollCoordinationRole::ViewportConstrained;
    case ScrollingNodeType::Positioned:
        return ScrollCoordinationRole::Positioning;
    }
    ASSERT_NOT_REACHED();
    return ScrollCoordinationRole::Scrolling;
}

ScrollingNodeID RenderLayerCompositor::attachScrollingNode(RenderLayer& layer, ScrollingNodeType nodeType, ScrollingTreeState& treeState)
{
    auto* scrollingCoordinator = this->scrollingCoordinator();
    auto* backing = layer.backing();
    // Crash logs suggest that backing can be null here, but we don't know how: rdar://problem/18545452.
    ASSERT(backing);
    if (!backing)
        return 0;

    ASSERT(treeState.parentNodeID || nodeType == ScrollingNodeType::Subframe);
    ASSERT_IMPLIES(nodeType == ScrollingNodeType::MainFrame, !treeState.parentNodeID.value());

    ScrollCoordinationRole role = scrollCoordinationRoleForNodeType(nodeType);
    ScrollingNodeID nodeID = backing->scrollingNodeIDForRole(role);
    
    nodeID = registerScrollingNodeID(*scrollingCoordinator, nodeID, nodeType, treeState);

    LOG_WITH_STREAM(ScrollingTree, stream << "RenderLayerCompositor " << this << " attachScrollingNode " << nodeID << " (layer " << backing->graphicsLayer()->primaryLayerID() << ") type " << nodeType << " parent " << treeState.parentNodeID.value_or(0));

    if (!nodeID)
        return 0;
    
    backing->setScrollingNodeIDForRole(nodeID, role);

#if ENABLE(SCROLLING_THREAD)
    if (nodeType == ScrollingNodeType::Subframe)
        m_clipLayer->setScrollingNodeID(nodeID);
#endif

    m_scrollingNodeToLayerMap.add(nodeID, layer);

    return nodeID;
}

ScrollingNodeID RenderLayerCompositor::registerScrollingNodeID(ScrollingCoordinator& scrollingCoordinator, ScrollingNodeID nodeID, ScrollingNodeType nodeType, struct ScrollingTreeState& treeState)
{
    if (!nodeID)
        nodeID = scrollingCoordinator.uniqueScrollingNodeID();

    if (nodeType == ScrollingNodeType::Subframe && !treeState.parentNodeID)
        nodeID = scrollingCoordinator.createNode(nodeType, nodeID);
    else {
        auto newNodeID = scrollingCoordinator.insertNode(nodeType, nodeID, treeState.parentNodeID.value_or(0), treeState.nextChildIndex);
        if (newNodeID != nodeID) {
            // We'll get a new nodeID if the type changed (and not if the node is new).
            scrollingCoordinator.unparentChildrenAndDestroyNode(nodeID);
            m_scrollingNodeToLayerMap.remove(nodeID);
        }
        nodeID = newNodeID;
    }

    ASSERT(nodeID);
    if (!nodeID)
        return 0;
    
    ++treeState.nextChildIndex;
    return nodeID;
}

void RenderLayerCompositor::detachScrollCoordinatedLayerWithRole(RenderLayer& layer, ScrollingCoordinator& scrollingCoordinator, ScrollCoordinationRole role)
{
    auto unregisterNode = [&](ScrollingNodeID nodeID) {
        auto childNodes = scrollingCoordinator.childrenOfNode(nodeID);
        for (auto childNodeID : childNodes) {
            if (auto weakLayer = m_scrollingNodeToLayerMap.get(childNodeID))
                weakLayer->setNeedsScrollingTreeUpdate();
        }

        m_scrollingNodeToLayerMap.remove(nodeID);
    };

    if (role == ScrollCoordinationRole::ScrollingProxy) {
        ASSERT(layer.isComposited());
        auto* clippingStack = layer.backing()->ancestorClippingStack();
        if (!clippingStack)
            return;
        
        auto& stack = clippingStack->stack();
        for (auto& entry : stack) {
            if (entry.overflowScrollProxyNodeID)
                unregisterNode(entry.overflowScrollProxyNodeID);
        }
        return;
    }

    auto nodeID = layer.backing()->scrollingNodeIDForRole(role);
    if (!nodeID)
        return;

    unregisterNode(nodeID);
}

void RenderLayerCompositor::detachScrollCoordinatedLayer(RenderLayer& layer, OptionSet<ScrollCoordinationRole> roles)
{
    auto* backing = layer.backing();
    if (!backing)
        return;

    auto* scrollingCoordinator = this->scrollingCoordinator();

    if (roles.contains(ScrollCoordinationRole::Scrolling))
        detachScrollCoordinatedLayerWithRole(layer, *scrollingCoordinator, ScrollCoordinationRole::Scrolling);

    if (roles.contains(ScrollCoordinationRole::ScrollingProxy))
        detachScrollCoordinatedLayerWithRole(layer, *scrollingCoordinator, ScrollCoordinationRole::ScrollingProxy);

    if (roles.contains(ScrollCoordinationRole::FrameHosting))
        detachScrollCoordinatedLayerWithRole(layer, *scrollingCoordinator, ScrollCoordinationRole::FrameHosting);

    if (roles.contains(ScrollCoordinationRole::ViewportConstrained))
        detachScrollCoordinatedLayerWithRole(layer, *scrollingCoordinator, ScrollCoordinationRole::ViewportConstrained);

    if (roles.contains(ScrollCoordinationRole::Positioning))
        detachScrollCoordinatedLayerWithRole(layer, *scrollingCoordinator, ScrollCoordinationRole::Positioning);

    backing->detachFromScrollingCoordinator(roles);
}

OptionSet<ScrollCoordinationRole> RenderLayerCompositor::coordinatedScrollingRolesForLayer(const RenderLayer& layer, const RenderLayer* compositingAncestor) const
{
    OptionSet<ScrollCoordinationRole> coordinationRoles;
    if (isViewportConstrainedFixedOrStickyLayer(layer))
        coordinationRoles.add(ScrollCoordinationRole::ViewportConstrained);

    if (useCoordinatedScrollingForLayer(layer))
        coordinationRoles.add(ScrollCoordinationRole::Scrolling);

    auto coordinatedPositioning = computeCoordinatedPositioningForLayer(layer, compositingAncestor);
    switch (coordinatedPositioning) {
    case ScrollPositioningBehavior::Moves:
        coordinationRoles.add(ScrollCoordinationRole::ScrollingProxy);
        break;
    case ScrollPositioningBehavior::Stationary:
        coordinationRoles.add(ScrollCoordinationRole::Positioning);
        break;
    case ScrollPositioningBehavior::None:
        break;
    }

    if (isLayerForIFrameWithScrollCoordinatedContents(layer))
        coordinationRoles.add(ScrollCoordinationRole::FrameHosting);

    return coordinationRoles;
}

ScrollingNodeID RenderLayerCompositor::updateScrollCoordinationForLayer(RenderLayer& layer, const RenderLayer* compositingAncestor, ScrollingTreeState& treeState, OptionSet<ScrollingNodeChangeFlags> changes)
{
    auto roles = coordinatedScrollingRolesForLayer(layer, compositingAncestor);

#if PLATFORM(IOS_FAMILY)
    if (m_legacyScrollingLayerCoordinator) {
        if (roles.contains(ScrollCoordinationRole::ViewportConstrained))
            m_legacyScrollingLayerCoordinator->addViewportConstrainedLayer(layer);
        else
            m_legacyScrollingLayerCoordinator->removeViewportConstrainedLayer(layer);
    }
#endif

    if (!hasCoordinatedScrolling()) {
        // If this frame isn't coordinated, it cannot contain any scrolling tree nodes.
        return 0;
    }

    auto newNodeID = treeState.parentNodeID.value_or(0);

    ScrollingTreeState childTreeState;
    ScrollingTreeState* currentTreeState = &treeState;

    // If there's a positioning node, it's the parent scrolling node for fixed/sticky/scrolling/frame hosting.
    if (roles.contains(ScrollCoordinationRole::Positioning)) {
        newNodeID = updateScrollingNodeForPositioningRole(layer, compositingAncestor, *currentTreeState, changes);
        childTreeState.parentNodeID = newNodeID;
        currentTreeState = &childTreeState;
    } else
        detachScrollCoordinatedLayer(layer, ScrollCoordinationRole::Positioning);

    // If there's a scrolling proxy node, it's the parent scrolling node for fixed/sticky/scrolling/frame hosting.
    if (roles.contains(ScrollCoordinationRole::ScrollingProxy)) {
        newNodeID = updateScrollingNodeForScrollingProxyRole(layer, *currentTreeState, changes);
        childTreeState.parentNodeID = newNodeID;
        currentTreeState = &childTreeState;
    } else
        detachScrollCoordinatedLayer(layer, ScrollCoordinationRole::ScrollingProxy);

    // If is fixed or sticky, it's the parent scrolling node for scrolling/frame hosting.
    if (roles.contains(ScrollCoordinationRole::ViewportConstrained)) {
        newNodeID = updateScrollingNodeForViewportConstrainedRole(layer, *currentTreeState, changes);
        // ViewportConstrained nodes are the parent of same-layer scrolling nodes, so adjust the ScrollingTreeState.
        childTreeState.parentNodeID = newNodeID;
        currentTreeState = &childTreeState;
    } else
        detachScrollCoordinatedLayer(layer, ScrollCoordinationRole::ViewportConstrained);

    if (roles.contains(ScrollCoordinationRole::Scrolling))
        newNodeID = updateScrollingNodeForScrollingRole(layer, *currentTreeState, changes);
    else
        detachScrollCoordinatedLayer(layer, ScrollCoordinationRole::Scrolling);

    if (roles.contains(ScrollCoordinationRole::FrameHosting))
        newNodeID = updateScrollingNodeForFrameHostingRole(layer, *currentTreeState, changes);
    else
        detachScrollCoordinatedLayer(layer, ScrollCoordinationRole::FrameHosting);

    return newNodeID;
}

ScrollingNodeID RenderLayerCompositor::updateScrollingNodeForViewportConstrainedRole(RenderLayer& layer, ScrollingTreeState& treeState, OptionSet<ScrollingNodeChangeFlags> changes)
{
    auto* scrollingCoordinator = this->scrollingCoordinator();

    auto nodeType = ScrollingNodeType::Fixed;
    if (layer.renderer().style().position() == PositionType::Sticky)
        nodeType = ScrollingNodeType::Sticky;
    else
        ASSERT(layer.renderer().isFixedPositioned());

    auto newNodeID = attachScrollingNode(layer, nodeType, treeState);
    if (!newNodeID) {
        ASSERT_NOT_REACHED();
        return treeState.parentNodeID.value_or(0);
    }

    LOG_WITH_STREAM(Compositing, stream << "Registering ViewportConstrained " << nodeType << " node " << newNodeID << " (layer " << layer.backing()->graphicsLayer()->primaryLayerID() << ") as child of " << treeState.parentNodeID.value_or(0));

    if (changes & ScrollingNodeChangeFlags::Layer) {
        ASSERT(layer.backing()->viewportAnchorLayer());
        scrollingCoordinator->setNodeLayers(newNodeID, { layer.backing()->viewportAnchorLayer() });
    }

    if (changes & ScrollingNodeChangeFlags::LayerGeometry) {
        switch (nodeType) {
        case ScrollingNodeType::Fixed:
            scrollingCoordinator->setViewportConstraintedNodeConstraints(newNodeID, computeFixedViewportConstraints(layer));
            break;
        case ScrollingNodeType::Sticky:
            scrollingCoordinator->setViewportConstraintedNodeConstraints(newNodeID, computeStickyViewportConstraints(layer));
            break;
        default:
            break;
        }
    }

    return newNodeID;
}

RoundedRect RenderLayerCompositor::parentRelativeScrollableRect(const RenderLayer& layer, const RenderLayer* ancestorLayer) const
{
    // FIXME: ancestorLayer needs to be always non-null, so should become a reference.
    if (!ancestorLayer) {
        if (!layer.scrollableArea())
            return RoundedRect { LayoutRect { } };
        return RoundedRect { LayoutRect({ }, LayoutSize(layer.scrollableArea()->visibleSize())) };
    }

    if (!is<RenderBox>(layer.renderer()))
        return RoundedRect { LayoutRect { } };

    auto& box = downcast<RenderBox>(layer.renderer());
    auto scrollableRect = RoundedRect { box.paddingBoxRect() };
    if (box.style().hasBorderRadius())
        scrollableRect = box.style().getRoundedInnerBorderFor(box.borderBoxRect());

    auto offset = layer.convertToLayerCoords(ancestorLayer, scrollableRect.rect().location()); // FIXME: broken for columns.
    auto rect = scrollableRect.rect();
    rect.setLocation(offset);
    scrollableRect.setRect(rect);
    return scrollableRect;
}

void RenderLayerCompositor::updateScrollingNodeLayers(ScrollingNodeID nodeID, RenderLayer& layer, ScrollingCoordinator& scrollingCoordinator)
{
    if (layer.isRenderViewLayer()) {
        FrameView& frameView = m_renderView.frameView();
        scrollingCoordinator.setNodeLayers(nodeID, { nullptr,
            scrollContainerLayer(), scrolledContentsLayer(),
            fixedRootBackgroundLayer(), clipLayer(), rootContentsLayer(),
            frameView.layerForHorizontalScrollbar(), frameView.layerForVerticalScrollbar() });
    } else {
        auto* scrollableArea = layer.scrollableArea();
        ASSERT(scrollableArea);

        auto& backing = *layer.backing();
        scrollingCoordinator.setNodeLayers(nodeID, { backing.graphicsLayer(),
            backing.scrollContainerLayer(), backing.scrolledContentsLayer(),
            nullptr, nullptr, nullptr,
            scrollableArea->layerForHorizontalScrollbar(), scrollableArea->layerForVerticalScrollbar() });
    }
}

ScrollingNodeID RenderLayerCompositor::updateScrollingNodeForScrollingRole(RenderLayer& layer, ScrollingTreeState& treeState, OptionSet<ScrollingNodeChangeFlags> changes)
{
    auto* scrollingCoordinator = this->scrollingCoordinator();

    ScrollingNodeID newNodeID = 0;

    if (layer.isRenderViewLayer()) {
        FrameView& frameView = m_renderView.frameView();
        ASSERT_UNUSED(frameView, scrollingCoordinator->coordinatesScrollingForFrameView(frameView));

        newNodeID = attachScrollingNode(*m_renderView.layer(), m_renderView.frame().isMainFrame() ? ScrollingNodeType::MainFrame : ScrollingNodeType::Subframe, treeState);

        if (!newNodeID) {
            ASSERT_NOT_REACHED();
            return treeState.parentNodeID.value_or(0);
        }

        if (changes & ScrollingNodeChangeFlags::Layer)
            updateScrollingNodeLayers(newNodeID, layer, *scrollingCoordinator);

        if (changes & ScrollingNodeChangeFlags::LayerGeometry) {
            scrollingCoordinator->setScrollingNodeScrollableAreaGeometry(newNodeID, frameView);
            scrollingCoordinator->setFrameScrollingNodeState(newNodeID, frameView);
        }
    } else {
        newNodeID = attachScrollingNode(layer, ScrollingNodeType::Overflow, treeState);
        if (!newNodeID) {
            ASSERT_NOT_REACHED();
            return treeState.parentNodeID.value_or(0);
        }
        
        if (changes & ScrollingNodeChangeFlags::Layer)
            updateScrollingNodeLayers(newNodeID, layer, *scrollingCoordinator);

        if (changes & ScrollingNodeChangeFlags::LayerGeometry && treeState.parentNodeID) {
            if (auto* scrollableArea = layer.scrollableArea())
                scrollingCoordinator->setScrollingNodeScrollableAreaGeometry(newNodeID, *scrollableArea);
        }
    }

    return newNodeID;
}

ScrollingNodeID RenderLayerCompositor::updateScrollingNodeForScrollingProxyRole(RenderLayer& layer, ScrollingTreeState& treeState, OptionSet<ScrollingNodeChangeFlags> changes)
{
    auto* scrollingCoordinator = this->scrollingCoordinator();
    auto* clippingStack = layer.backing()->ancestorClippingStack();
    if (!clippingStack)
        return treeState.parentNodeID.value_or(0);

    ScrollingNodeID nodeID = 0;
    for (auto& entry : clippingStack->stack()) {
        if (!entry.clipData.isOverflowScroll)
            continue;

        nodeID = registerScrollingNodeID(*scrollingCoordinator, entry.overflowScrollProxyNodeID, ScrollingNodeType::OverflowProxy, treeState);
        if (!nodeID) {
            ASSERT_NOT_REACHED();
            return treeState.parentNodeID.value_or(0);
        }
        entry.overflowScrollProxyNodeID = nodeID;
#if ENABLE(SCROLLING_THREAD)
        if (entry.scrollingLayer)
            entry.scrollingLayer->setScrollingNodeID(nodeID);
#endif

        if (changes & ScrollingNodeChangeFlags::Layer)
            scrollingCoordinator->setNodeLayers(entry.overflowScrollProxyNodeID, { entry.scrollingLayer.get() });

        if (changes & ScrollingNodeChangeFlags::LayerGeometry) {
            ASSERT(entry.clipData.clippingLayer);
            ASSERT(entry.clipData.clippingLayer->isComposited());

            ScrollingNodeID overflowScrollNodeID = 0;
            if (auto* backing = entry.clipData.clippingLayer->backing())
                overflowScrollNodeID = backing->scrollingNodeIDForRole(ScrollCoordinationRole::Scrolling);

            Vector<ScrollingNodeID> scrollingNodeIDs;
            if (overflowScrollNodeID)
                scrollingNodeIDs.append(overflowScrollNodeID);
            scrollingCoordinator->setRelatedOverflowScrollingNodes(entry.overflowScrollProxyNodeID, WTFMove(scrollingNodeIDs));
        }
    }
    
    // FIXME: also m_overflowControlsHostLayerAncestorClippingStack

    if (!nodeID)
        return treeState.parentNodeID.value_or(0);

    return nodeID;
}

ScrollingNodeID RenderLayerCompositor::updateScrollingNodeForFrameHostingRole(RenderLayer& layer, ScrollingTreeState& treeState, OptionSet<ScrollingNodeChangeFlags> changes)
{
    auto* scrollingCoordinator = this->scrollingCoordinator();

    auto newNodeID = attachScrollingNode(layer, ScrollingNodeType::FrameHosting, treeState);
    if (!newNodeID) {
        ASSERT_NOT_REACHED();
        return treeState.parentNodeID.value_or(0);
    }

    if (changes & ScrollingNodeChangeFlags::Layer)
        scrollingCoordinator->setNodeLayers(newNodeID, { layer.backing()->graphicsLayer() });

    return newNodeID;
}

ScrollingNodeID RenderLayerCompositor::updateScrollingNodeForPositioningRole(RenderLayer& layer, const RenderLayer* compositingAncestor, ScrollingTreeState& treeState, OptionSet<ScrollingNodeChangeFlags> changes)
{
    auto* scrollingCoordinator = this->scrollingCoordinator();

    auto newNodeID = attachScrollingNode(layer, ScrollingNodeType::Positioned, treeState);
    if (!newNodeID) {
        ASSERT_NOT_REACHED();
        return treeState.parentNodeID.value_or(0);
    }

    if (changes & ScrollingNodeChangeFlags::Layer) {
        auto& backing = *layer.backing();
        scrollingCoordinator->setNodeLayers(newNodeID, { backing.graphicsLayer() });
    }

    if (changes & ScrollingNodeChangeFlags::LayerGeometry && treeState.parentNodeID) {
        // Would be nice to avoid calling computeCoordinatedPositioningForLayer() again.
        auto positioningBehavior = computeCoordinatedPositioningForLayer(layer, compositingAncestor);
        auto relatedNodeIDs = collectRelatedCoordinatedScrollingNodes(layer, positioningBehavior);
        scrollingCoordinator->setRelatedOverflowScrollingNodes(newNodeID, WTFMove(relatedNodeIDs));

        auto* graphicsLayer = layer.backing()->graphicsLayer();
        AbsolutePositionConstraints constraints;
        constraints.setAlignmentOffset(graphicsLayer->pixelAlignmentOffset());
        constraints.setLayerPositionAtLastLayout(graphicsLayer->position());
        scrollingCoordinator->setPositionedNodeConstraints(newNodeID, constraints);
    }

    return newNodeID;
}

void RenderLayerCompositor::updateSynchronousScrollingNodes()
{
    if (!hasCoordinatedScrolling())
        return;

    if (m_renderView.settings().fixedBackgroundsPaintRelativeToDocument())
        return;

    auto scrollingCoordinator = this->scrollingCoordinator();
    ASSERT(scrollingCoordinator);

    auto rootScrollingNodeID = m_renderView.frameView().scrollingNodeID();
    HashSet<ScrollingNodeID> nodesToClear;
    nodesToClear.reserveInitialCapacity(m_scrollingNodeToLayerMap.size());
    for (auto key : m_scrollingNodeToLayerMap.keys())
        nodesToClear.add(key);
    
    auto clearSynchronousReasonsOnNonRootNodes = [&] {
        for (auto nodeID : nodesToClear) {
            if (nodeID == rootScrollingNodeID)
                continue;

            // Harmless to call setSynchronousScrollingReasons on a non-scrolling node.
            scrollingCoordinator->setSynchronousScrollingReasons(nodeID, { });
        }
    };
    
    auto setHasSlowRepaintObjectsSynchronousScrollingReasonOnRootNode = [&](bool hasSlowRepaintObjects) {
        // ScrollingCoordinator manages all bits other than HasSlowRepaintObjects, so maintain their current value.
        auto reasons = scrollingCoordinator->synchronousScrollingReasons(rootScrollingNodeID);
        reasons.set({ SynchronousScrollingReason::HasSlowRepaintObjects }, hasSlowRepaintObjects);
        scrollingCoordinator->setSynchronousScrollingReasons(rootScrollingNodeID, reasons);
    };

    auto slowRepaintObjects = m_renderView.frameView().slowRepaintObjects();
    if (!slowRepaintObjects) {
        setHasSlowRepaintObjectsSynchronousScrollingReasonOnRootNode(false);
        clearSynchronousReasonsOnNonRootNodes();
        return;
    }

    auto relevantScrollingScope = [](const RenderObject& renderer, const RenderLayer& layer) {
        if (&layer.renderer() == &renderer)
            return layer.boxScrollingScope();
        return layer.contentsScrollingScope();
    };

    bool rootHasSlowRepaintObjects = false;
    for (auto& renderer : *slowRepaintObjects) {
        auto layer = renderer.enclosingLayer();
        if (!layer)
            continue;

        auto scrollingScope = relevantScrollingScope(renderer, *layer);
        if (!scrollingScope)
            continue;

        if (auto enclosingScrollingNodeID = asyncScrollableContainerNodeID(renderer)) {
            LOG_WITH_STREAM(Scrolling, stream << "RenderLayerCompositor::updateSynchronousScrollingNodes - node " << enclosingScrollingNodeID << " slow-scrolling because of fixed backgrounds");
            ASSERT(enclosingScrollingNodeID != rootScrollingNodeID);

            scrollingCoordinator->setSynchronousScrollingReasons(enclosingScrollingNodeID, { SynchronousScrollingReason::HasSlowRepaintObjects });
            nodesToClear.remove(enclosingScrollingNodeID);

            // Although the root scrolling layer does not have a slow repaint object in it directly,
            // we need to set some synchronous scrolling reason on it so that
            // ScrollingCoordinator::shouldUpdateScrollLayerPositionSynchronously returns an
            // appropriate value. (Scrolling itself would be correct without this, since the
            // scrolling tree propagates DescendantScrollersHaveSynchronousScrolling bits up the
            // tree, but shouldUpdateScrollLayerPositionSynchronously looks at the scrolling state
            // tree instead.)
            rootHasSlowRepaintObjects = true;
        } else if (!layer->behavesAsFixed()) {
            LOG_WITH_STREAM(Scrolling, stream << "RenderLayerCompositor::updateSynchronousScrollingNodes - root node slow-scrolling because of fixed backgrounds");
            rootHasSlowRepaintObjects = true;
        }
    }

    setHasSlowRepaintObjectsSynchronousScrollingReasonOnRootNode(rootHasSlowRepaintObjects);
    clearSynchronousReasonsOnNonRootNodes();
}

ScrollableArea* RenderLayerCompositor::scrollableAreaForScrollingNodeID(ScrollingNodeID nodeID) const
{
    if (!nodeID)
        return nullptr;

    if (nodeID == m_renderView.frameView().scrollingNodeID())
        return &m_renderView.frameView();

    return m_scrollingNodeToLayerMap.get(nodeID)->scrollableArea();
}

void RenderLayerCompositor::willRemoveScrollingLayerWithBacking(RenderLayer& layer, RenderLayerBacking& backing)
{
    if (scrollingCoordinator())
        return;

#if PLATFORM(IOS_FAMILY)
    ASSERT(m_renderView.document().backForwardCacheState() == Document::NotInBackForwardCache);
    if (m_legacyScrollingLayerCoordinator)
        m_legacyScrollingLayerCoordinator->removeScrollingLayer(layer, backing);
#else
    UNUSED_PARAM(layer);
    UNUSED_PARAM(backing);
#endif
}

// FIXME: This should really be called from the updateBackingAndHierarchy.
void RenderLayerCompositor::didAddScrollingLayer(RenderLayer& layer)
{
    if (scrollingCoordinator())
        return;

#if PLATFORM(IOS_FAMILY)
    ASSERT(m_renderView.document().backForwardCacheState() == Document::NotInBackForwardCache);
    if (m_legacyScrollingLayerCoordinator)
        m_legacyScrollingLayerCoordinator->addScrollingLayer(layer);
#else
    UNUSED_PARAM(layer);
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

void RenderLayerCompositor::updateScrollSnapPropertiesWithFrameView(const FrameView& frameView) const
{
    if (auto* coordinator = scrollingCoordinator())
        coordinator->updateScrollSnapPropertiesWithFrameView(frameView);
}

Page& RenderLayerCompositor::page() const
{
    return m_renderView.page();
}

TextStream& operator<<(TextStream& ts, CompositingUpdateType updateType)
{
    switch (updateType) {
    case CompositingUpdateType::AfterStyleChange: ts << "after style change"; break;
    case CompositingUpdateType::AfterLayout: ts << "after layout"; break;
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

TextStream& operator<<(TextStream& ts, CompositingReason compositingReason)
{
    return ts << compositingReasonToString(compositingReason);
}

#if PLATFORM(IOS_FAMILY)
typedef HashMap<PlatformLayer*, std::unique_ptr<ViewportConstraints>> LayerMap;
typedef HashMap<PlatformLayer*, PlatformLayer*> StickyContainerMap;

void LegacyWebKitScrollingLayerCoordinator::registerAllViewportConstrainedLayers(RenderLayerCompositor& compositor)
{
    if (!m_coordinateViewportConstrainedLayers)
        return;

    LayerMap layerMap;
    StickyContainerMap stickyContainerMap;

    for (auto* layer : m_viewportConstrainedLayers) {
        ASSERT(layer->isComposited());

        std::unique_ptr<ViewportConstraints> constraints;
        if (layer->renderer().isStickilyPositioned()) {
            constraints = makeUnique<StickyPositionViewportConstraints>(compositor.computeStickyViewportConstraints(*layer));
            const RenderLayer* enclosingTouchScrollableLayer = nullptr;
            if (compositor.isAsyncScrollableStickyLayer(*layer, &enclosingTouchScrollableLayer) && enclosingTouchScrollableLayer) {
                ASSERT(enclosingTouchScrollableLayer->isComposited());
                // what
                stickyContainerMap.add(layer->backing()->graphicsLayer()->platformLayer(), enclosingTouchScrollableLayer->backing()->scrollContainerLayer()->platformLayer());
            }
        } else if (layer->renderer().isFixedPositioned())
            constraints = makeUnique<FixedPositionViewportConstraints>(compositor.computeFixedViewportConstraints(*layer));
        else
            continue;

        layerMap.add(layer->backing()->graphicsLayer()->platformLayer(), WTFMove(constraints));
    }
    
    m_chromeClient.updateViewportConstrainedLayers(layerMap, stickyContainerMap);
}

void LegacyWebKitScrollingLayerCoordinator::unregisterAllViewportConstrainedLayers()
{
    if (!m_coordinateViewportConstrainedLayers)
        return;

    LayerMap layerMap;
    m_chromeClient.updateViewportConstrainedLayers(layerMap, { });
}

void LegacyWebKitScrollingLayerCoordinator::updateScrollingLayer(RenderLayer& layer)
{
    auto* backing = layer.backing();
    ASSERT(backing);

    auto* scrollableArea = layer.scrollableArea();
    ASSERT(scrollableArea);

    bool allowHorizontalScrollbar = !scrollableArea->horizontalScrollbarHiddenByStyle();
    bool allowVerticalScrollbar = !scrollableArea->verticalScrollbarHiddenByStyle();

    m_chromeClient.addOrUpdateScrollingLayer(layer.renderer().element(), backing->scrollContainerLayer()->platformLayer(), backing->scrolledContentsLayer()->platformLayer(),
        scrollableArea->reachableTotalContentsSize(), allowHorizontalScrollbar, allowVerticalScrollbar);
}

void LegacyWebKitScrollingLayerCoordinator::registerAllScrollingLayers()
{
    for (auto* layer : m_scrollingLayers)
        updateScrollingLayer(*layer);
}

void LegacyWebKitScrollingLayerCoordinator::unregisterAllScrollingLayers()
{
    for (auto* layer : m_scrollingLayers) {
        auto* backing = layer->backing();
        ASSERT(backing);
        m_chromeClient.removeScrollingLayer(layer->renderer().element(), backing->scrollContainerLayer()->platformLayer(), backing->scrolledContentsLayer()->platformLayer());
    }
}

void LegacyWebKitScrollingLayerCoordinator::addScrollingLayer(RenderLayer& layer)
{
    m_scrollingLayers.add(&layer);
}

void LegacyWebKitScrollingLayerCoordinator::removeScrollingLayer(RenderLayer& layer, RenderLayerBacking& backing)
{
    if (m_scrollingLayers.remove(&layer)) {
        auto* scrollContainerLayer = backing.scrollContainerLayer()->platformLayer();
        auto* scrolledContentsLayer = backing.scrolledContentsLayer()->platformLayer();
        m_chromeClient.removeScrollingLayer(layer.renderer().element(), scrollContainerLayer, scrolledContentsLayer);
    }
}

void LegacyWebKitScrollingLayerCoordinator::removeLayer(RenderLayer& layer)
{
    removeScrollingLayer(layer, *layer.backing());

    // We'll put the new set of layers to the client via registerAllViewportConstrainedLayers() at flush time.
    m_viewportConstrainedLayers.remove(&layer);
}

void LegacyWebKitScrollingLayerCoordinator::addViewportConstrainedLayer(RenderLayer& layer)
{
    m_viewportConstrainedLayers.add(&layer);
}

void LegacyWebKitScrollingLayerCoordinator::removeViewportConstrainedLayer(RenderLayer& layer)
{
    m_viewportConstrainedLayers.remove(&layer);
}

#endif

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
void showGraphicsLayerTreeForCompositor(WebCore::RenderLayerCompositor& compositor)
{
    showGraphicsLayerTree(compositor.rootGraphicsLayer());
}
#endif
