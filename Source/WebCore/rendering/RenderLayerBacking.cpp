/*
 * Copyright (C) 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "RenderLayerBacking.h"

#include "BackgroundPainter.h"
#include "BitmapImage.h"
#include "CanvasRenderingContext.h"
#include "CSSPropertyNames.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "DebugOverlayRegions.h"
#include "DebugPageOverlays.h"
#include "EventRegion.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLMediaElement.h"
#include "HTMLModelElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "InspectorInstrumentation.h"
#include "KeyframeList.h"
#include "LayerAncestorClippingStack.h"
#include "Logging.h"
#include "Model.h"
#include "NullGraphicsContext.h"
#include "Page.h"
#include "PerformanceLoggingClient.h"
#include "PluginViewBase.h"
#include "ProgressTracker.h"
#include "RenderAncestorIterator.h"
#include "RenderEmbeddedObject.h"
#include "RenderFragmentContainer.h"
#include "RenderFragmentedFlow.h"
#include "RenderHTMLCanvas.h"
#include "RenderIFrame.h"
#include "RenderImage.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderMedia.h"
#include "RenderModel.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGModelObject.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "Styleable.h"
#include "TiledBacking.h"
#include <wtf/SystemTracing.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "RuntimeApplicationChecks.h"
#endif

#if PLATFORM(MAC)
#include "LocalDefaultSystemAppearance.h"
#endif

namespace WebCore {

using namespace HTMLNames;

CanvasCompositingStrategy canvasCompositingStrategy(const RenderObject& renderer)
{
    ASSERT(renderer.isCanvas());
    
    const HTMLCanvasElement* canvas = downcast<HTMLCanvasElement>(renderer.node());
    auto* context = canvas->renderingContext();
    if (!context || !context->isAccelerated())
        return UnacceleratedCanvas;
    
    if (context->isGPUBased())
        return CanvasAsLayerContents;

    return CanvasPaintedToLayer; // On Mac and iOS we paint accelerated canvases into their layers.
}

// This acts as a cache of what we know about what is painting into this RenderLayerBacking.
class PaintedContentsInfo {
public:
    enum class ContentsTypeDetermination {
        Unknown,
        SimpleContainer,
        DirectlyCompositedImage,
        UnscaledBitmapOnly,
        Painted,
    };

    PaintedContentsInfo(RenderLayerBacking& inBacking)
        : m_backing(inBacking)
    {
    }

    RequestState paintsBoxDecorationsDetermination();
    bool paintsBoxDecorations()
    {
        RequestState state = paintsBoxDecorationsDetermination();
        return state == RequestState::True || state == RequestState::Undetermined;
    }

    RequestState paintsContentDetermination();
    bool paintsContent()
    {
        RequestState state = paintsContentDetermination();
        return state == RequestState::True || state == RequestState::Undetermined;
    }

    ContentsTypeDetermination contentsTypeDetermination();
    bool isSimpleContainer()
    {
        return contentsTypeDetermination() == ContentsTypeDetermination::SimpleContainer;
    }

    bool isDirectlyCompositedImage()
    {
        return contentsTypeDetermination() == ContentsTypeDetermination::DirectlyCompositedImage;
    }

    bool isUnscaledBitmapOnly()
    {
        return contentsTypeDetermination() == ContentsTypeDetermination::UnscaledBitmapOnly;
    }

    RenderLayerBacking& m_backing;
    RequestState m_boxDecorations { RequestState::Unknown };
    RequestState m_content { RequestState::Unknown };

    ContentsTypeDetermination m_contentsType { ContentsTypeDetermination::Unknown };
};

RequestState PaintedContentsInfo::paintsBoxDecorationsDetermination()
{
    if (m_boxDecorations != RequestState::Unknown)
        return m_boxDecorations;

    m_boxDecorations = m_backing.paintsBoxDecorations() ? RequestState::True : RequestState::False;
    return m_boxDecorations;
}

RequestState PaintedContentsInfo::paintsContentDetermination()
{
    if (m_content != RequestState::Unknown)
        return m_content;

    RenderLayer::PaintedContentRequest contentRequest;
    m_content = m_backing.paintsContent(contentRequest) ? RequestState::True : RequestState::False;

    return m_content;
}

PaintedContentsInfo::ContentsTypeDetermination PaintedContentsInfo::contentsTypeDetermination()
{
    if (m_contentsType != ContentsTypeDetermination::Unknown)
        return m_contentsType;

    if (m_backing.isSimpleContainerCompositingLayer(*this))
        m_contentsType = ContentsTypeDetermination::SimpleContainer;
    else if (m_backing.isDirectlyCompositedImage())
        m_contentsType = ContentsTypeDetermination::DirectlyCompositedImage;
    else if (m_backing.isUnscaledBitmapOnly())
        m_contentsType = ContentsTypeDetermination::UnscaledBitmapOnly;
    else
        m_contentsType = ContentsTypeDetermination::Painted;

    return m_contentsType;
}


RenderLayerBacking::RenderLayerBacking(RenderLayer& layer)
    : m_owningLayer(layer)
{
    if (layer.isRenderViewLayer()) {
        m_isMainFrameRenderViewLayer = renderer().frame().isMainFrame();
        m_isFrameLayerWithTiledBacking = renderer().page().chrome().client().shouldUseTiledBackingForFrameView(renderer().view().frameView());
    }
    
    createPrimaryGraphicsLayer();
#if ENABLE(FULLSCREEN_API)
    setRequiresBackgroundLayer(layer.renderer().isRenderFullScreen());
#endif

    if (auto* tiledBacking = this->tiledBacking()) {
        tiledBacking->setIsInWindow(renderer().page().isInWindow());

        if (m_isFrameLayerWithTiledBacking) {
            tiledBacking->setScrollingPerformanceTestingEnabled(renderer().settings().scrollingPerformanceTestingEnabled());
            adjustTiledBackingCoverage();
        }
    }
}

RenderLayerBacking::~RenderLayerBacking()
{
#if USE(OWNING_LAYER_BEAR_TRAP)
    RELEASE_ASSERT_WITH_MESSAGE(m_owningLayerBearTrap == BEAR_TRAP_VALUE, "~RenderLayerBacking: m_owningLayerBearTrap caught the bear (55699292)");
    RELEASE_ASSERT_WITH_MESSAGE(&m_owningLayer, "~RenderLayerBacking: m_owningLayer is null (55699292)");
#endif
    // Note that m_owningLayer->backing() is null here.
    updateAncestorClipping(false, nullptr);
    updateDescendantClippingLayer(false);
    updateOverflowControlsLayers(false, false, false);
    updateForegroundLayer(false);
    updateBackgroundLayer(false);
    updateMaskingLayer(false, false);
    updateScrollingLayers(false);
    
    ASSERT(!m_viewportConstrainedNodeID);
    ASSERT(!m_scrollingNodeID);
    ASSERT(!m_frameHostingNodeID);
    ASSERT(!m_positioningNodeID);

    destroyGraphicsLayers();
}

void RenderLayerBacking::willBeDestroyed()
{
#if USE(OWNING_LAYER_BEAR_TRAP)
    RELEASE_ASSERT_WITH_MESSAGE(m_owningLayerBearTrap == BEAR_TRAP_VALUE, "RenderLayerBacking::willBeDestroyed(): m_owningLayerBearTrap caught the bear (55699292)");
    RELEASE_ASSERT_WITH_MESSAGE(&m_owningLayer, "RenderLayerBacking::willBeDestroyed(): m_owningLayer is null (55699292)");
#endif

    ASSERT(m_owningLayer.backing() == this);
    compositor().removeFromScrollCoordinatedLayers(m_owningLayer);

    clearBackingSharingLayers();
}

void RenderLayerBacking::willDestroyLayer(const GraphicsLayer* layer)
{
    if (layer && layer->type() == GraphicsLayer::Type::Normal && layer->tiledBacking())
        compositor().layerTiledBackingUsageChanged(layer, false);
}

static void clearBackingSharingLayerProviders(Vector<WeakPtr<RenderLayer>>& sharingLayers, const RenderLayer& providerLayer)
{
    for (auto& layerWeakPtr : sharingLayers) {
        if (!layerWeakPtr)
            continue;
        if (layerWeakPtr->backingProviderLayer() == &providerLayer)
            layerWeakPtr->setBackingProviderLayer(nullptr);
    }
}

void RenderLayerBacking::setBackingSharingLayers(Vector<WeakPtr<RenderLayer>>&& sharingLayers)
{
    bool sharingLayersChanged = m_backingSharingLayers != sharingLayers;
    if (sharingLayersChanged) {
        // For layers that used to share and no longer do, and are not composited, recompute repaint rects.
        for (auto& oldSharingLayer : m_backingSharingLayers) {
            // Layers that go from shared to composited have their repaint rects recomputed in RenderLayerCompositor::updateBacking().
            // FIXME: Two O(n^2) traversals in this funtion. Probably OK because sharing lists are usually small, but still.
            if (!sharingLayers.contains(oldSharingLayer) && !oldSharingLayer->isComposited())
                oldSharingLayer->computeRepaintRectsIncludingDescendants();
        }
    }

    clearBackingSharingLayerProviders(m_backingSharingLayers, m_owningLayer);

    if (sharingLayers != m_backingSharingLayers) {
        if (sharingLayers.size())
            setRequiresOwnBackingStore(true);
        setContentsNeedDisplay(); // This could be optimized to only repaint rects for changed layers.
    }

    auto oldSharingLayers = WTFMove(m_backingSharingLayers);
    m_backingSharingLayers = WTFMove(sharingLayers);

    for (auto& layerWeakPtr : m_backingSharingLayers)
        layerWeakPtr->setBackingProviderLayer(&m_owningLayer);

    if (sharingLayersChanged) {
        // For layers that are newly sharing, recompute repaint rects.
        for (auto& currentSharingLayer : m_backingSharingLayers) {
            if (!oldSharingLayers.contains(currentSharingLayer))
                currentSharingLayer->computeRepaintRectsIncludingDescendants();
        }
    }
}

void RenderLayerBacking::removeBackingSharingLayer(RenderLayer& layer)
{
    layer.setBackingProviderLayer(nullptr);
    m_backingSharingLayers.removeAll(&layer);
}

void RenderLayerBacking::clearBackingSharingLayers()
{
    clearBackingSharingLayerProviders(m_backingSharingLayers, m_owningLayer);
    m_backingSharingLayers.clear();
}

Ref<GraphicsLayer> RenderLayerBacking::createGraphicsLayer(const String& name, GraphicsLayer::Type layerType)
{
    auto* graphicsLayerFactory = renderer().page().chrome().client().graphicsLayerFactory();

    auto graphicsLayer = GraphicsLayer::create(graphicsLayerFactory, *this, layerType);

    graphicsLayer->setName(name);

#if PLATFORM(COCOA) && USE(CA)
    graphicsLayer->setAcceleratesDrawing(compositor().acceleratedDrawingEnabled());
    graphicsLayer->setUsesDisplayListDrawing(compositor().displayListDrawingEnabled());
#endif

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (renderer().isSVGLayerAwareRenderer() && renderer().document().settings().layerBasedSVGEngineEnabled())
        graphicsLayer->setShouldUpdateRootRelativeScaleFactor(true);
#endif
    
    return graphicsLayer;
}

void RenderLayerBacking::setUsesDisplayListDrawing(bool usesDisplayListDrawing)
{
    // Note that this only affects the primary layer.
    if (usesDisplayListDrawing == m_graphicsLayer->usesDisplayListDrawing())
        return;

    m_graphicsLayer->setUsesDisplayListDrawing(usesDisplayListDrawing);
    if (m_graphicsLayer->drawsContent())
        m_graphicsLayer->setNeedsDisplay();
}

String RenderLayerBacking::displayListAsText(OptionSet<DisplayList::AsTextFlag> flags) const
{
    return m_graphicsLayer->displayListAsText(flags);
}

void RenderLayerBacking::setIsTrackingDisplayListReplay(bool isTrackingReplay)
{
    m_graphicsLayer->setIsTrackingDisplayListReplay(isTrackingReplay);
}

String RenderLayerBacking::replayDisplayListAsText(OptionSet<DisplayList::AsTextFlag> flags) const
{
    return m_graphicsLayer->replayDisplayListAsText(flags);
}

void RenderLayerBacking::tiledBackingUsageChanged(const GraphicsLayer* layer, bool usingTiledBacking)
{
    compositor().layerTiledBackingUsageChanged(layer, usingTiledBacking);
}

TiledBacking* RenderLayerBacking::tiledBacking() const
{
    return m_graphicsLayer->tiledBacking();
}

static TiledBacking::TileCoverage computePageTiledBackingCoverage(const RenderLayer& layer)
{
    // If the page is non-visible, don't incur the cost of keeping extra tiles for scrolling.
    if (!layer.page().isVisible())
        return TiledBacking::CoverageForVisibleArea;

    auto& frameView = layer.renderer().view().frameView();

    TiledBacking::TileCoverage tileCoverage = TiledBacking::CoverageForVisibleArea;
    bool useMinimalTilesDuringLiveResize = frameView.inLiveResize();
    if (frameView.speculativeTilingEnabled() && !useMinimalTilesDuringLiveResize) {
        bool clipsToExposedRect = static_cast<bool>(frameView.viewExposedRect());
        if (frameView.horizontalScrollbarMode() != ScrollbarMode::AlwaysOff || clipsToExposedRect)
            tileCoverage |= TiledBacking::CoverageForHorizontalScrolling;

        if (frameView.verticalScrollbarMode() != ScrollbarMode::AlwaysOff || clipsToExposedRect)
            tileCoverage |= TiledBacking::CoverageForVerticalScrolling;
    }
    return tileCoverage;
}

static TiledBacking::TileCoverage computeOverflowTiledBackingCoverage(const RenderLayer& layer)
{
    // If the page is non-visible, don't incur the cost of keeping extra tiles for scrolling.
    if (!layer.page().isVisible())
        return TiledBacking::CoverageForVisibleArea;
    
    auto& frameView = layer.renderer().view().frameView();

    TiledBacking::TileCoverage tileCoverage = TiledBacking::CoverageForVisibleArea;
    bool useMinimalTilesDuringLiveResize = frameView.inLiveResize();
    if (!useMinimalTilesDuringLiveResize) {
        if (auto* scrollableArea = layer.scrollableArea()) {
            if (scrollableArea->hasScrollableHorizontalOverflow())
                tileCoverage |= TiledBacking::CoverageForHorizontalScrolling;

            if (scrollableArea->hasScrollableVerticalOverflow())
                tileCoverage |= TiledBacking::CoverageForVerticalScrolling;
        }
    }
    return tileCoverage;
}

void RenderLayerBacking::adjustTiledBackingCoverage()
{
    if (m_isFrameLayerWithTiledBacking) {
        auto tileCoverage = computePageTiledBackingCoverage(m_owningLayer);
        if (auto* tiledBacking = this->tiledBacking())
            tiledBacking->setTileCoverage(tileCoverage);
    }

    if (m_owningLayer.hasCompositedScrollableOverflow() && m_scrolledContentsLayer) {
        if (auto* tiledBacking = m_scrolledContentsLayer->tiledBacking()) {
            auto tileCoverage = computeOverflowTiledBackingCoverage(m_owningLayer);
            tiledBacking->setTileCoverage(tileCoverage);
        }
    }
}

void RenderLayerBacking::setTiledBackingHasMargins(bool hasExtendedBackgroundOnLeftAndRight, bool hasExtendedBackgroundOnTopAndBottom)
{
    if (!m_isFrameLayerWithTiledBacking)
        return;

    tiledBacking()->setHasMargins(hasExtendedBackgroundOnTopAndBottom, hasExtendedBackgroundOnTopAndBottom, hasExtendedBackgroundOnLeftAndRight, hasExtendedBackgroundOnLeftAndRight);
}

void RenderLayerBacking::updateDebugIndicators(bool showBorder, bool showRepaintCounter)
{
    m_graphicsLayer->setShowDebugBorder(showBorder);
    m_graphicsLayer->setShowRepaintCounter(showRepaintCounter);

    // m_viewportAnchorLayer can't show layer borders becuase it's a structural layer.

    if (m_ancestorClippingStack) {
        for (auto& entry : m_ancestorClippingStack->stack())
            entry.clippingLayer->setShowDebugBorder(showBorder);
    }

    if (m_foregroundLayer) {
        m_foregroundLayer->setShowDebugBorder(showBorder);
        m_foregroundLayer->setShowRepaintCounter(showRepaintCounter);
    }
    
    if (m_contentsContainmentLayer)
        m_contentsContainmentLayer->setShowDebugBorder(showBorder);

    if (m_childContainmentLayer)
        m_childContainmentLayer->setShowDebugBorder(showBorder);

    if (m_backgroundLayer) {
        m_backgroundLayer->setShowDebugBorder(showBorder);
        m_backgroundLayer->setShowRepaintCounter(showRepaintCounter);
    }

    if (m_maskLayer) {
        m_maskLayer->setShowDebugBorder(showBorder);
        m_maskLayer->setShowRepaintCounter(showRepaintCounter);
    }

    if (m_layerForHorizontalScrollbar)
        m_layerForHorizontalScrollbar->setShowDebugBorder(showBorder);

    if (m_layerForVerticalScrollbar)
        m_layerForVerticalScrollbar->setShowDebugBorder(showBorder);

    if (m_layerForScrollCorner)
        m_layerForScrollCorner->setShowDebugBorder(showBorder);

    if (m_scrollContainerLayer)
        m_scrollContainerLayer->setShowDebugBorder(showBorder);

    if (m_scrolledContentsLayer) {
        m_scrolledContentsLayer->setShowDebugBorder(showBorder);
        m_scrolledContentsLayer->setShowRepaintCounter(showRepaintCounter);
    }

    if (m_overflowControlsContainer)
        m_overflowControlsContainer->setShowDebugBorder(showBorder);
}

void RenderLayerBacking::createPrimaryGraphicsLayer()
{
    String layerName = m_owningLayer.name();
    const unsigned maxLayerNameLength = 100;
    if (layerName.length() > maxLayerNameLength)
        layerName = makeString(StringView(layerName).left(maxLayerNameLength), "..."_s);
    m_graphicsLayer = createGraphicsLayer(layerName, m_isFrameLayerWithTiledBacking ? GraphicsLayer::Type::PageTiledBacking : GraphicsLayer::Type::Normal);

    if (m_isFrameLayerWithTiledBacking) {
        m_childContainmentLayer = createGraphicsLayer("Page TiledBacking containment"_s);
        m_graphicsLayer->addChild(*m_childContainmentLayer);
    }

#if !PLATFORM(IOS_FAMILY)
    if (m_isMainFrameRenderViewLayer) {
        // Page scale is applied above the RenderView on iOS.
        m_graphicsLayer->setContentsOpaque(!compositor().viewHasTransparentBackground());
        m_graphicsLayer->setAppliesPageScale();
    }
#endif

#if PLATFORM(COCOA) && USE(CA)
    if (!compositor().acceleratedDrawingEnabled() && renderer().isCanvas()) {
        const HTMLCanvasElement* canvas = downcast<HTMLCanvasElement>(renderer().element());
        if (canvas->shouldAccelerate(canvas->size()))
            m_graphicsLayer->setAcceleratesDrawing(true);
    }
#endif    
    auto& style = renderer().style();
    updateOpacity(style);
    updateTransform(style);
    updateFilters(style);
#if ENABLE(FILTERS_LEVEL_2)
    updateBackdropFilters(style);
#endif
#if ENABLE(CSS_COMPOSITING)
    updateBlendMode(style);
#endif
    updateContentsScalingFilters(style);
}

bool RenderLayerBacking::shouldSetContentsDisplayDelegate() const
{
    if (!renderer().isCanvas())
        return false;

    if (canvasCompositingStrategy(renderer()) != CanvasAsLayerContents)
        return false;

#if ENABLE(WEBGL) || ENABLE(OFFSCREEN_CANVAS)
    return true;
#else
    return renderer().settings().webGPU();
#endif
}

#if PLATFORM(IOS_FAMILY)
void RenderLayerBacking::layerWillBeDestroyed()
{
    auto& renderer = this->renderer();
    if (is<RenderEmbeddedObject>(renderer) && downcast<RenderEmbeddedObject>(renderer).allowsAcceleratedCompositing()) {
        auto* pluginViewBase = downcast<PluginViewBase>(downcast<RenderWidget>(renderer).widget());
        if (pluginViewBase && m_graphicsLayer->contentsLayerForMedia())
            pluginViewBase->detachPluginLayer();
    }
}

bool RenderLayerBacking::needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack(const GraphicsLayer& layer) const
{
    if (m_isMainFrameRenderViewLayer && IOSApplication::isDumpRenderTree()) {
        // In iOS WebKit1 the main frame's RenderView layer is always transparent. We lie that it is opaque so that
        // internals.layerTreeAsText() tests succeed.
        ASSERT_UNUSED(layer, !layer.contentsOpaque());
        return true;
    }
    return false;
}
#endif

void RenderLayerBacking::destroyGraphicsLayers()
{
    if (m_graphicsLayer) {
        m_graphicsLayer->setMaskLayer(nullptr);
        m_graphicsLayer->setReplicatedByLayer(nullptr);
        willDestroyLayer(m_graphicsLayer.get());
    }

    GraphicsLayer::clear(m_maskLayer);

    if (m_ancestorClippingStack)
        removeClippingStackLayers(*m_ancestorClippingStack);

    if (m_overflowControlsHostLayerAncestorClippingStack)
        removeClippingStackLayers(*m_overflowControlsHostLayerAncestorClippingStack);

    GraphicsLayer::unparentAndClear(m_transformFlatteningLayer);
    GraphicsLayer::unparentAndClear(m_viewportAnchorLayer);
    GraphicsLayer::unparentAndClear(m_contentsContainmentLayer);
    GraphicsLayer::unparentAndClear(m_foregroundLayer);
    GraphicsLayer::unparentAndClear(m_backgroundLayer);
    GraphicsLayer::unparentAndClear(m_childContainmentLayer);
    GraphicsLayer::unparentAndClear(m_scrollContainerLayer);
    GraphicsLayer::unparentAndClear(m_scrolledContentsLayer);
    GraphicsLayer::unparentAndClear(m_graphicsLayer);
}

static LayoutRect scrollContainerLayerBox(const RenderBox& renderBox)
{
    return renderBox.paddingBoxRect();
}

static LayoutRect clippingLayerBox(const RenderLayerModelObject& renderer)
{
    LayoutRect result = LayoutRect::infiniteRect();
    if (renderer.hasNonVisibleOverflow()) {
        if (is<RenderBox>(renderer))
            result = downcast<RenderBox>(renderer).overflowClipRect({ }, 0); // FIXME: Incorrect for CSS regions.
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        else if (is<RenderSVGModelObject>(renderer))
            result = downcast<RenderSVGModelObject>(renderer).overflowClipRect({ }, 0); // FIXME: Incorrect for CSS regions.
#endif
    }

    if (renderer.hasClip() && is<RenderBox>(renderer))
        result.intersect(downcast<RenderBox>(renderer).clipRect({ }, 0)); // FIXME: Incorrect for CSS regions.

    return result;
}

static LayoutRect overflowControlsHostLayerRect(const RenderBox& renderBox)
{
    return renderBox.paddingBoxRectIncludingScrollbar();
}

void RenderLayerBacking::updateOpacity(const RenderStyle& style)
{
    m_graphicsLayer->setOpacity(compositingOpacity(style.opacity()));
}

void RenderLayerBacking::updateTransform(const RenderStyle& style)
{
    TransformationMatrix t;
    if (m_owningLayer.hasTransform())
        m_owningLayer.updateTransformFromStyle(t, style, RenderStyle::individualTransformOperations);
    
    if (m_contentsContainmentLayer) {
        m_contentsContainmentLayer->setTransform(t);
        m_graphicsLayer->setTransform({ });
    } else
        m_graphicsLayer->setTransform(t);
}

void RenderLayerBacking::updateChildrenTransformAndAnchorPoint(const LayoutRect& primaryGraphicsLayerRect, LayoutSize offsetFromParentGraphicsLayer)
{
    auto defaultAnchorPoint = FloatPoint3D { 0.5, 0.5, 0 };
    if (!renderer().hasTransformRelatedProperty()) {
        m_graphicsLayer->setAnchorPoint(defaultAnchorPoint);
        if (m_contentsContainmentLayer)
            m_contentsContainmentLayer->setAnchorPoint(defaultAnchorPoint);

        if (m_childContainmentLayer)
            m_childContainmentLayer->setAnchorPoint(defaultAnchorPoint);

        if (m_scrollContainerLayer)
            m_scrollContainerLayer->setAnchorPoint(defaultAnchorPoint);

        if (m_scrolledContentsLayer)
            m_scrolledContentsLayer->setPreserves3D(false);
        return;
    }

    const auto deviceScaleFactor = this->deviceScaleFactor();
    auto transformOrigin = m_owningLayer.transformOriginPixelSnappedIfNeeded();
    auto layerOffset = roundPointToDevicePixels(toLayoutPoint(offsetFromParentGraphicsLayer), deviceScaleFactor);
    auto anchor = FloatPoint3D {
        primaryGraphicsLayerRect.width() ? ((layerOffset.x() - primaryGraphicsLayerRect.x()) + transformOrigin.x()) / primaryGraphicsLayerRect.width() : 0.5f,
        primaryGraphicsLayerRect.height() ? ((layerOffset.y() - primaryGraphicsLayerRect.y())+ transformOrigin.y()) / primaryGraphicsLayerRect.height() : 0.5f,
        transformOrigin.z()
    };

    if (m_contentsContainmentLayer)
        m_contentsContainmentLayer->setAnchorPoint(anchor);
    else
        m_graphicsLayer->setAnchorPoint(anchor);

    auto removeChildrenTransformFromLayers = [&](GraphicsLayer* layerToIgnore = nullptr) {
        auto* clippingLayer = this->clippingLayer();
        if (clippingLayer && clippingLayer != layerToIgnore) {
            clippingLayer->setChildrenTransform({ });
            clippingLayer->setAnchorPoint(defaultAnchorPoint);
        }

        if (m_scrollContainerLayer && m_scrollContainerLayer != layerToIgnore) {
            m_scrollContainerLayer->setChildrenTransform({ });
            m_scrollContainerLayer->setAnchorPoint(defaultAnchorPoint);
            m_scrolledContentsLayer->setPreserves3D(false);
        }

        if (m_graphicsLayer != layerToIgnore)
            m_graphicsLayer->setChildrenTransform({ });
    };

    if (!renderer().style().hasPerspective()) {
        removeChildrenTransformFromLayers();
        return;
    }

    auto layerForChildrenTransform = [&]() -> std::tuple<GraphicsLayer*, FloatRect> {
        if (m_scrollContainerLayer) {
            ASSERT(is<RenderBox>(renderer())); // Scroll container layers are only created for RenderBox derived renderers.
            return std::make_tuple(m_scrollContainerLayer.get(), scrollContainerLayerBox(downcast<RenderBox>(renderer())));
        }
        if (auto* layer = clippingLayer())
            return std::make_tuple(layer, clippingLayerBox(renderer()));

        return std::make_tuple(m_graphicsLayer.get(), renderer().transformReferenceBoxRect());
    };

    auto [layerForPerspective, layerForPerspectiveRect] = layerForChildrenTransform();
    if (layerForPerspective != m_graphicsLayer) {
        // If we have scrolling layers, we need the children transform on m_scrollContainerLayer to
        // affect children of m_scrolledContentsLayer, so set setPreserves3D(true).
        if (layerForPerspective == m_scrollContainerLayer)
            m_scrolledContentsLayer->setPreserves3D(true);

        auto perspectiveAnchorPoint = FloatPoint3D {
            layerForPerspectiveRect.width() ? (transformOrigin.x() - layerForPerspectiveRect.x()) / layerForPerspectiveRect.width() : 0.5f,
            layerForPerspectiveRect.height() ? (transformOrigin.y() - layerForPerspectiveRect.y()) / layerForPerspectiveRect.height() : 0.5f,
            transformOrigin.z()
        };

        layerForPerspective->setAnchorPoint(perspectiveAnchorPoint);
    }

    layerForPerspective->setChildrenTransform(m_owningLayer.perspectiveTransform());
    removeChildrenTransformFromLayers(layerForPerspective);
}

void RenderLayerBacking::updateFilters(const RenderStyle& style)
{
    m_canCompositeFilters = m_graphicsLayer->setFilters(style.filter());
}

#if ENABLE(FILTERS_LEVEL_2)
void RenderLayerBacking::updateBackdropFilters(const RenderStyle& style)
{
    m_canCompositeBackdropFilters = m_graphicsLayer->setBackdropFilters(style.backdropFilter());
}

void RenderLayerBacking::updateBackdropFiltersGeometry()
{
    if (!m_canCompositeBackdropFilters)
        return;

    if (!is<RenderBox>(renderer()))
        return;

    auto& renderBox = downcast<RenderBox>(this->renderer());

    FloatRoundedRect backdropFiltersRect;
    if (renderBox.style().hasBorderRadius() && !renderBox.hasClip()) {
        auto roundedBoxRect = renderBox.roundedBorderBoxRect();
        roundedBoxRect.move(contentOffsetInCompositingLayer());
        backdropFiltersRect = roundedBoxRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor());
    } else {
        auto boxRect = renderBox.borderBoxRect();
        if (renderBox.hasClip())
            boxRect.intersect(renderBox.clipRect(LayoutPoint(), nullptr));
        boxRect.move(contentOffsetInCompositingLayer());
        backdropFiltersRect = FloatRoundedRect(snapRectToDevicePixels(boxRect, deviceScaleFactor()));
    }

    m_graphicsLayer->setBackdropFiltersRect(backdropFiltersRect);
}
#endif

#if ENABLE(CSS_COMPOSITING)
void RenderLayerBacking::updateBlendMode(const RenderStyle& style)
{
    // FIXME: where is the blend mode updated when m_ancestorClippingStacks come and go?
    if (m_ancestorClippingStack) {
        m_ancestorClippingStack->stack().first().clippingLayer->setBlendMode(style.blendMode());
        m_graphicsLayer->setBlendMode(BlendMode::Normal);
    } else
        m_graphicsLayer->setBlendMode(style.blendMode());
}
#endif

void RenderLayerBacking::updateContentsScalingFilters(const RenderStyle& style)
{
    if (!renderer().isCanvas() || canvasCompositingStrategy(renderer()) != CanvasAsLayerContents)
        return;
    auto minificationFilter = GraphicsLayer::ScalingFilter::Linear;
    auto magnificationFilter = GraphicsLayer::ScalingFilter::Linear;
    switch (style.imageRendering()) {
    case ImageRendering::CrispEdges:
    case ImageRendering::Pixelated:
        // FIXME: In order to match other code-paths, we treat these the same.
        minificationFilter = GraphicsLayer::ScalingFilter::Nearest;
        magnificationFilter = GraphicsLayer::ScalingFilter::Nearest;
        break;
    default:
        break;
    }
    m_graphicsLayer->setContentsMinificationFilter(minificationFilter);
    m_graphicsLayer->setContentsMagnificationFilter(magnificationFilter);
}

static bool layerOrAncestorIsTransformedOrUsingCompositedScrolling(RenderLayer& layer)
{
    for (auto* curr = &layer; curr; curr = curr->parent()) {
        if (curr->hasTransform() || curr->hasCompositedScrollableOverflow())
            return true;
    }

    return false;
}

bool RenderLayerBacking::shouldClipCompositedBounds() const
{
#if !PLATFORM(IOS_FAMILY)
    // Scrollbar layers use this layer for relative positioning, so don't clip.
    if (layerForHorizontalScrollbar() || layerForVerticalScrollbar())
        return false;
#endif

    if (m_isFrameLayerWithTiledBacking)
        return false;

    if (layerOrAncestorIsTransformedOrUsingCompositedScrolling(m_owningLayer))
        return false;

    return true;
}

static bool hasNonZeroTransformOrigin(const RenderObject& renderer)
{
    const RenderStyle& style = renderer.style();
    return (style.transformOriginX().isFixed() && style.transformOriginX().value())
        || (style.transformOriginY().isFixed() && style.transformOriginY().value());
}

bool RenderLayerBacking::updateCompositedBounds()
{
#if USE(OWNING_LAYER_BEAR_TRAP)
    RELEASE_ASSERT_WITH_MESSAGE(m_owningLayerBearTrap == BEAR_TRAP_VALUE, "RenderLayerBacking::updateCompositedBounds(): m_owningLayerBearTrap caught the bear (55699292)");
    RELEASE_ASSERT_WITH_MESSAGE(&m_owningLayer, "RenderLayerBacking::updateCompositedBounds(): m_owningLayer is null (55699292)");
#endif

    LayoutRect layerBounds = m_owningLayer.calculateLayerBounds(&m_owningLayer, { }, RenderLayer::defaultCalculateLayerBoundsFlags() | RenderLayer::ExcludeHiddenDescendants | RenderLayer::DontConstrainForMask);
    // Clip to the size of the document or enclosing overflow-scroll layer.
    // If this or an ancestor is transformed, we can't currently compute the correct rect to intersect with.
    // We'd need RenderObject::convertContainerToLocalQuad(), which doesn't yet exist.
    if (shouldClipCompositedBounds()) {
        auto& view = renderer().view();
        auto* rootLayer = view.layer();

        LayoutRect clippingBounds;
        if (renderer().isFixedPositioned() && renderer().container() == &view)
            clippingBounds = view.frameView().rectForFixedPositionLayout();
        else
            clippingBounds = view.unscaledDocumentRect();

        if (&m_owningLayer != rootLayer)
            clippingBounds.intersect(m_owningLayer.backgroundClipRect(RenderLayer::ClipRectsContext(rootLayer, AbsoluteClipRects)).rect()); // FIXME: Incorrect for CSS regions.

        LayoutPoint delta = m_owningLayer.convertToLayerCoords(rootLayer, LayoutPoint(), RenderLayer::AdjustForColumns);
        clippingBounds.move(-delta.x(), -delta.y());

        layerBounds.intersect(clippingBounds);
    }

    // If the backing provider has overflow:clip, we know all sharing layers are affected by the clip because they are containing-block descendants.
    if (!renderer().hasNonVisibleOverflow()) {
        for (auto& layerWeakPtr : m_backingSharingLayers) {
            auto* boundsRootLayer = &m_owningLayer;
            ASSERT(layerWeakPtr->isDescendantOf(m_owningLayer));
            auto offset = layerWeakPtr->offsetFromAncestor(&m_owningLayer);
            auto bounds = layerWeakPtr->calculateLayerBounds(boundsRootLayer, offset, RenderLayer::defaultCalculateLayerBoundsFlags() | RenderLayer::ExcludeHiddenDescendants | RenderLayer::DontConstrainForMask);
            layerBounds.unite(bounds);
        }
    }

    // If the element has a transform-origin that has fixed lengths, and the renderer has zero size,
    // then we need to ensure that the compositing layer has non-zero size so that we can apply
    // the transform-origin via the GraphicsLayer anchorPoint (which is expressed as a fractional value).
    if (layerBounds.isEmpty() && (hasNonZeroTransformOrigin(renderer()) || renderer().style().hasPerspective())) {
        layerBounds.setWidth(1);
        layerBounds.setHeight(1);
        m_artificiallyInflatedBounds = true;
    } else
        m_artificiallyInflatedBounds = false;

    return setCompositedBounds(layerBounds);
}

void RenderLayerBacking::updateAllowsBackingStoreDetaching(const LayoutRect& absoluteBounds)
{
    auto setAllowsBackingStoreDetaching = [&](bool allowDetaching) {
        m_graphicsLayer->setAllowsBackingStoreDetaching(allowDetaching);
        if (m_foregroundLayer)
            m_foregroundLayer->setAllowsBackingStoreDetaching(allowDetaching);
        if (m_backgroundLayer)
            m_backgroundLayer->setAllowsBackingStoreDetaching(allowDetaching);
        if (m_scrolledContentsLayer)
            m_scrolledContentsLayer->setAllowsBackingStoreDetaching(allowDetaching);
    };

    if (!m_owningLayer.behavesAsFixed()) {
        setAllowsBackingStoreDetaching(true);
        return;
    }

    // We'll allow detaching if the layer is outside the layout viewport. Fixed layers inside
    // the layout viewport can be revealed by async scrolling, so we want to pin their backing store.
    FrameView& frameView = renderer().view().frameView();
    LayoutRect fixedLayoutRect;
    if (frameView.useFixedLayout())
        fixedLayoutRect = renderer().view().unscaledDocumentRect();
    else
        fixedLayoutRect = frameView.rectForFixedPositionLayout();

    bool allowDetaching = !fixedLayoutRect.intersects(absoluteBounds);
    LOG_WITH_STREAM(Compositing, stream << "RenderLayerBacking (layer " << &m_owningLayer << ") updateAllowsBackingStoreDetaching - absoluteBounds " << absoluteBounds << " layoutViewportRect " << fixedLayoutRect << ", allowDetaching " << allowDetaching);
    setAllowsBackingStoreDetaching(allowDetaching);
}

void RenderLayerBacking::updateAfterWidgetResize()
{
    if (!is<RenderWidget>(renderer()))
        return;

    if (auto* innerCompositor = RenderLayerCompositor::frameContentsCompositor(downcast<RenderWidget>(renderer()))) {
        innerCompositor->frameViewDidChangeSize();
        innerCompositor->frameViewDidChangeLocation(flooredIntPoint(contentsBox().location()));
    }
}

void RenderLayerBacking::updateAfterLayout(bool needsClippingUpdate, bool needsFullRepaint)
{
#if USE(OWNING_LAYER_BEAR_TRAP)
    RELEASE_ASSERT_WITH_MESSAGE(m_owningLayerBearTrap == BEAR_TRAP_VALUE, "RenderLayerBacking::updateAfterLayout(): m_owningLayerBearTrap caught the bear (55699292)");
    RELEASE_ASSERT_WITH_MESSAGE(&m_owningLayer, "RenderLayerBacking::updateAfterLayout(): m_owningLayer is null (55699292)");
#endif

    LOG_WITH_STREAM(Compositing, stream << "RenderLayerBacking::updateAfterLayout (layer " << &m_owningLayer << " needsClippingUpdate " << needsClippingUpdate << " needsFullRepaint " << needsFullRepaint);

    // This is the main trigger for layout changing layer geometry, but we have to do the work again in updateBackingAndHierarchy()
    // when we know the final compositing hierarchy. We can't just set dirty bits from RenderLayer::setSize() because that doesn't
    // take overflow into account.
    if (updateCompositedBounds()) {
        m_owningLayer.setNeedsCompositingGeometryUpdate();
        // This layer's geometry affects those of its children.
        m_owningLayer.setChildrenNeedCompositingGeometryUpdate();
    } else if (needsClippingUpdate) {
        m_owningLayer.setNeedsCompositingConfigurationUpdate();
        m_owningLayer.setNeedsCompositingGeometryUpdate();
    }
    
    if (needsFullRepaint && canIssueSetNeedsDisplay())
        setContentsNeedDisplay();
}

// This can only update things that don't require up-to-date layout.
void RenderLayerBacking::updateConfigurationAfterStyleChange()
{
    updateMaskingLayer(renderer().hasMask(), renderer().hasClipPath());

    if (m_owningLayer.hasReflection()) {
        if (m_owningLayer.reflectionLayer()->backing()) {
            auto* reflectionLayer = m_owningLayer.reflectionLayer()->backing()->graphicsLayer();
            m_graphicsLayer->setReplicatedByLayer(reflectionLayer);
        }
    } else
        m_graphicsLayer->setReplicatedByLayer(nullptr);

    // FIXME: do we care if opacity is animating?
    auto& style = renderer().style();
    updateOpacity(style);
    updateFilters(style);

#if ENABLE(FILTERS_LEVEL_2)
    updateBackdropFilters(style);
#endif
#if ENABLE(CSS_COMPOSITING)
    updateBlendMode(style);
#endif
    updateContentsScalingFilters(style);
}

bool RenderLayerBacking::updateConfiguration(const RenderLayer* compositingAncestor)
{
    ASSERT(!m_owningLayer.normalFlowListDirty());
    ASSERT(!m_owningLayer.zOrderListsDirty());
    ASSERT(!renderer().view().needsLayout());

    bool layerConfigChanged = false;
    auto& compositor = this->compositor();

    if (updateTransformFlatteningLayer(compositingAncestor))
        layerConfigChanged = true;

    if (updateViewportConstrainedAnchorLayer(compositor.isViewportConstrainedFixedOrStickyLayer(m_owningLayer)))
        layerConfigChanged = true;

    setBackgroundLayerPaintsFixedRootBackground(compositor.needsFixedRootBackgroundLayer(m_owningLayer));

    if (updateBackgroundLayer(m_backgroundLayerPaintsFixedRootBackground || m_requiresBackgroundLayer))
        layerConfigChanged = true;

    if (updateForegroundLayer(compositor.needsContentsCompositingLayer(m_owningLayer)))
        layerConfigChanged = true;
    
    bool needsDescendantsClippingLayer = false;
    bool usesCompositedScrolling = m_owningLayer.hasCompositedScrollableOverflow();

    if (usesCompositedScrolling) {
        // If it's scrollable, it has to be a box.
        FloatRoundedRect contentsClippingRect = downcast<RenderBox>(renderer()).roundedBorderBoxRect().pixelSnappedRoundedRectForPainting(deviceScaleFactor());
        needsDescendantsClippingLayer = contentsClippingRect.isRounded();
    } else
        needsDescendantsClippingLayer = RenderLayerCompositor::clipsCompositingDescendants(m_owningLayer);

    if (updateScrollingLayers(usesCompositedScrolling))
        layerConfigChanged = true;

    if (updateDescendantClippingLayer(needsDescendantsClippingLayer))
        layerConfigChanged = true;

    ASSERT(compositingAncestor == m_owningLayer.ancestorCompositingLayer());
    if (updateAncestorClipping(compositor.clippedByAncestor(m_owningLayer, compositingAncestor), compositingAncestor))
        layerConfigChanged = true;

    if (updateOverflowControlsLayers(requiresHorizontalScrollbarLayer(), requiresVerticalScrollbarLayer(), requiresScrollCornerLayer()))
        layerConfigChanged = true;

    if (layerConfigChanged)
        updateInternalHierarchy();

    // RenderLayerCompositor::adjustOverflowScrollbarContainerLayers() may have reparented the overflowControlsContainer
    // in an earlier update, so always put it back here. We don't yet know if it will get reparented again.
    if (m_overflowControlsContainer && m_overflowControlsContainer->parent() != m_graphicsLayer.get()) {
        m_graphicsLayer->addChild(*m_overflowControlsContainer);
        // Ensure that we fix up the position of m_overflowControlsContainer.
        m_owningLayer.setNeedsCompositingGeometryUpdate();
    }

    // FIXME: Overlow controls need to be above the flattening layer?
    if (auto* flatteningLayer = tileCacheFlatteningLayer()) {
        if (layerConfigChanged || flatteningLayer->parent() != m_graphicsLayer.get())
            m_graphicsLayer->addChild(*flatteningLayer);
    }

    if (updateMaskingLayer(renderer().hasMask(), renderer().hasClipPath()))
        layerConfigChanged = true;

    if (m_owningLayer.hasReflection()) {
        if (m_owningLayer.reflectionLayer()->backing()) {
            auto* reflectionLayer = m_owningLayer.reflectionLayer()->backing()->graphicsLayer();
            m_graphicsLayer->setReplicatedByLayer(reflectionLayer);
        }
    } else
        m_graphicsLayer->setReplicatedByLayer(nullptr);

    PaintedContentsInfo contentsInfo(*this);

    // Requires layout.
    if (!m_owningLayer.isRenderViewLayer()) {
        bool didUpdateContentsRect = false;
        updateDirectlyCompositedBoxDecorations(contentsInfo, didUpdateContentsRect);
    } else
        updateRootLayerConfiguration();

    // Requires layout.
    if (contentsInfo.isDirectlyCompositedImage())
        updateImageContents(contentsInfo);

    bool unscaledBitmap = contentsInfo.isUnscaledBitmapOnly();
    if (unscaledBitmap == m_graphicsLayer->appliesDeviceScale()) {
        m_graphicsLayer->setAppliesDeviceScale(!unscaledBitmap);
        layerConfigChanged = true;
    }

    if (is<RenderEmbeddedObject>(renderer()) && downcast<RenderEmbeddedObject>(renderer()).allowsAcceleratedCompositing()) {
        auto* pluginViewBase = downcast<PluginViewBase>(downcast<RenderWidget>(renderer()).widget());
#if PLATFORM(IOS_FAMILY)
        if (pluginViewBase && !m_graphicsLayer->contentsLayerForMedia()) {
            pluginViewBase->detachPluginLayer();
            pluginViewBase->attachPluginLayer();
        }
#else
        m_graphicsLayer->setContentsToPlatformLayer(pluginViewBase->platformLayer(), GraphicsLayer::ContentsLayerPurpose::Plugin);
#endif
    }
#if ENABLE(VIDEO)
    else if (is<RenderVideo>(renderer()) && downcast<RenderVideo>(renderer()).shouldDisplayVideo()) {
        auto* mediaElement = downcast<HTMLMediaElement>(renderer().element());
        m_graphicsLayer->setContentsToPlatformLayer(mediaElement->platformLayer(), GraphicsLayer::ContentsLayerPurpose::Media);
        updateContentsRects();
    }
#endif
    else if (shouldSetContentsDisplayDelegate()) {
        auto* canvas = downcast<HTMLCanvasElement>(renderer().element());
        if (auto* context = canvas->renderingContext())
            m_graphicsLayer->setContentsDisplayDelegate(context->layerContentsDisplayDelegate(), GraphicsLayer::ContentsLayerPurpose::Canvas);

        layerConfigChanged = true;
    }
#if ENABLE(MODEL_ELEMENT)
    else if (is<RenderModel>(renderer())) {
        auto element = downcast<HTMLModelElement>(renderer().element());

        // Some ModelPlayers use a platformLayer() and some pass the Model to the layer as contents,
        // but this is a runtime decision.
        if (element->usesPlatformLayer())
            m_graphicsLayer->setContentsToPlatformLayer(element->platformLayer(), GraphicsLayer::ContentsLayerPurpose::Model);
        else if (auto model = element->model())
            m_graphicsLayer->setContentsToModel(WTFMove(model), element->isInteractive() ? GraphicsLayer::ModelInteraction::Enabled : GraphicsLayer::ModelInteraction::Disabled);

        element->sizeMayHaveChanged();

        layerConfigChanged = true;
    }
#endif
    if (is<RenderWidget>(renderer()) && compositor.parentFrameContentLayers(downcast<RenderWidget>(renderer()))) {
        m_owningLayer.setNeedsCompositingGeometryUpdate();
        layerConfigChanged = true;
    }

    if (RenderLayerCompositor::isCompositedSubframeRenderer(renderer())) {
        m_graphicsLayer->setContentsRectClipsDescendants(true);
        updateContentsRects();
    }

    if (layerConfigChanged)
        updatePaintingPhases();

#if USE(OWNING_LAYER_BEAR_TRAP)
    RELEASE_ASSERT_WITH_MESSAGE(m_owningLayerBearTrap == BEAR_TRAP_VALUE, "RenderLayerBacking::updateConfiguration(): m_owningLayerBearTrap caught the bear (55699292)");
    RELEASE_ASSERT_WITH_MESSAGE(&m_owningLayer, "RenderLayerBacking::updateConfiguration(): m_owningLayer is null (55699292)");
#endif

    return layerConfigChanged;
}

static bool subpixelOffsetFromRendererChanged(const LayoutSize& oldSubpixelOffsetFromRenderer, const LayoutSize& newSubpixelOffsetFromRenderer, float deviceScaleFactor)
{
    FloatSize previous = snapSizeToDevicePixel(oldSubpixelOffsetFromRenderer, LayoutPoint(), deviceScaleFactor);
    FloatSize current = snapSizeToDevicePixel(newSubpixelOffsetFromRenderer, LayoutPoint(), deviceScaleFactor);
    return previous != current;
}

static FloatSize subpixelForLayerPainting(const LayoutPoint& point, float pixelSnappingFactor)
{
    LayoutUnit x = point.x();
    LayoutUnit y = point.y();
    x = x >= 0 ? floorToDevicePixel(x, pixelSnappingFactor) : ceilToDevicePixel(x, pixelSnappingFactor);
    y = y >= 0 ? floorToDevicePixel(y, pixelSnappingFactor) : ceilToDevicePixel(y, pixelSnappingFactor);
    return point - LayoutPoint(x, y);
}

struct OffsetFromRenderer {
    // 1.2px - > { m_devicePixelOffset = 1px m_subpixelOffset = 0.2px }
    LayoutSize m_devicePixelOffset;
    LayoutSize m_subpixelOffset;
};

static OffsetFromRenderer computeOffsetFromRenderer(const LayoutSize& offset, float deviceScaleFactor)
{
    OffsetFromRenderer offsetFromRenderer;
    offsetFromRenderer.m_subpixelOffset = LayoutSize(subpixelForLayerPainting(toLayoutPoint(offset), deviceScaleFactor));
    offsetFromRenderer.m_devicePixelOffset = offset - offsetFromRenderer.m_subpixelOffset;
    return offsetFromRenderer;
}

struct SnappedRectInfo {
    LayoutRect m_snappedRect;
    LayoutSize m_snapDelta;
};

static SnappedRectInfo snappedGraphicsLayer(const LayoutSize& offset, const LayoutSize& size, const RenderLayerModelObject& renderer)
{
    SnappedRectInfo snappedGraphicsLayer;
    LayoutRect graphicsLayerRect = LayoutRect(toLayoutPoint(offset), size);
    snappedGraphicsLayer.m_snappedRect = LayoutRect(snapRectToDevicePixelsIfNeeded(graphicsLayerRect, renderer));
    snappedGraphicsLayer.m_snapDelta = snappedGraphicsLayer.m_snappedRect.location() - toLayoutPoint(offset);
    return snappedGraphicsLayer;
}

static LayoutSize computeOffsetFromAncestorGraphicsLayer(const RenderLayer* compositedAncestor, const LayoutPoint& location, float deviceScaleFactor)
{
    if (!compositedAncestor)
        return toLayoutSize(location);

    // FIXME: This is a workaround until after webkit.org/b/162634 gets fixed. ancestorSubpixelOffsetFromRenderer
    // could be stale when a dynamic composited state change triggers a pre-order updateGeometry() traversal.
    LayoutSize ancestorSubpixelOffsetFromRenderer = compositedAncestor->backing()->subpixelOffsetFromRenderer();
    LayoutRect ancestorCompositedBounds = compositedAncestor->backing()->compositedBounds();
    LayoutSize floored = toLayoutSize(LayoutPoint(floorPointToDevicePixels(ancestorCompositedBounds.location() - ancestorSubpixelOffsetFromRenderer, deviceScaleFactor)));
    LayoutSize ancestorRendererOffsetFromAncestorGraphicsLayer = -(floored + ancestorSubpixelOffsetFromRenderer);
    return ancestorRendererOffsetFromAncestorGraphicsLayer + toLayoutSize(location);
}

class ComputedOffsets {
public:
    ComputedOffsets(const RenderLayer& renderLayer, const RenderLayer* compositingAncestor, const LayoutRect& localRect, const LayoutRect& parentGraphicsLayerRect, const LayoutRect& primaryGraphicsLayerRect)
        : m_renderLayer(renderLayer)
        , m_compositingAncestor(compositingAncestor)
        , m_location(localRect.location())
        , m_parentGraphicsLayerOffset(toLayoutSize(parentGraphicsLayerRect.location()))
        , m_primaryGraphicsLayerOffset(toLayoutSize(primaryGraphicsLayerRect.location()))
        , m_deviceScaleFactor(renderLayer.renderer().document().deviceScaleFactor())
    {
    }

    LayoutSize fromParentGraphicsLayer()
    {
        if (!m_fromParentGraphicsLayer)
            m_fromParentGraphicsLayer = fromAncestorGraphicsLayer() - m_parentGraphicsLayerOffset;
        return m_fromParentGraphicsLayer.value();
    }
    
    LayoutSize fromPrimaryGraphicsLayer()
    {
        if (!m_fromPrimaryGraphicsLayer)
            m_fromPrimaryGraphicsLayer = fromAncestorGraphicsLayer() - m_parentGraphicsLayerOffset - m_primaryGraphicsLayerOffset;
        return m_fromPrimaryGraphicsLayer.value();
    }
    
private:
    LayoutSize fromAncestorGraphicsLayer()
    {
        if (!m_fromAncestorGraphicsLayer) {
            LayoutPoint localPointInAncestorRenderLayerCoords = m_renderLayer.convertToLayerCoords(m_compositingAncestor, m_location, RenderLayer::AdjustForColumns);
            m_fromAncestorGraphicsLayer = computeOffsetFromAncestorGraphicsLayer(m_compositingAncestor, localPointInAncestorRenderLayerCoords, m_deviceScaleFactor);
        }
        return m_fromAncestorGraphicsLayer.value();
    }

    std::optional<LayoutSize> m_fromAncestorGraphicsLayer;
    std::optional<LayoutSize> m_fromParentGraphicsLayer;
    std::optional<LayoutSize> m_fromPrimaryGraphicsLayer;
    
    const RenderLayer& m_renderLayer;
    const RenderLayer* m_compositingAncestor;
    // Location is relative to the renderer.
    const LayoutPoint m_location;
    const LayoutSize m_parentGraphicsLayerOffset;
    const LayoutSize m_primaryGraphicsLayerOffset;
    float m_deviceScaleFactor;
};

LayoutRect RenderLayerBacking::computePrimaryGraphicsLayerRect(const RenderLayer* compositedAncestor, const LayoutRect& parentGraphicsLayerRect) const
{
    ComputedOffsets compositedBoundsOffset(m_owningLayer, compositedAncestor, compositedBounds(), parentGraphicsLayerRect, { });
    return LayoutRect(encloseRectToDevicePixels(LayoutRect(toLayoutPoint(compositedBoundsOffset.fromParentGraphicsLayer()), compositedBounds().size()),
        deviceScaleFactor()));
}

// FIXME: See if we need this now that updateGeometry() is always called in post-order traversal.
LayoutRect RenderLayerBacking::computeParentGraphicsLayerRect(const RenderLayer* compositedAncestor) const
{
    if (!compositedAncestor || !compositedAncestor->backing())
        return renderer().view().documentRect();

    auto* ancestorBacking = compositedAncestor->backing();
    LayoutRect parentGraphicsLayerRect;
    if (m_owningLayer.isInsideFragmentedFlow()) {
        // FIXME: flows/columns need work.
        LayoutRect ancestorCompositedBounds = ancestorBacking->compositedBounds();
        ancestorCompositedBounds.setLocation(LayoutPoint());
        parentGraphicsLayerRect = ancestorCompositedBounds;
    }

    if (!is<RenderBox>(compositedAncestor->renderer()))
        return parentGraphicsLayerRect;

    auto& ancestorRenderBox = downcast<RenderBox>(compositedAncestor->renderer());

    if (ancestorBacking->hasClippingLayer()) {
        // If the compositing ancestor has a layer to clip children, we parent in that, and therefore position relative to it.
        LayoutRect clippingBox = clippingLayerBox(ancestorRenderBox);
        LayoutSize clippingBoxOffset = computeOffsetFromAncestorGraphicsLayer(compositedAncestor, clippingBox.location(), deviceScaleFactor());
        parentGraphicsLayerRect = snappedGraphicsLayer(clippingBoxOffset, clippingBox.size(), renderer()).m_snappedRect;
    }

    if (compositedAncestor->hasCompositedScrollableOverflow()) {
        auto* scrollableArea = compositedAncestor->scrollableArea();
        ASSERT(scrollableArea);

        LayoutRect ancestorCompositedBounds = ancestorBacking->compositedBounds();
        LayoutRect scrollContainerBox = scrollContainerLayerBox(ancestorRenderBox);
        ScrollOffset scrollOffset = scrollableArea->scrollOffset();
        parentGraphicsLayerRect = LayoutRect((scrollContainerBox.location() - toLayoutSize(ancestorCompositedBounds.location()) - toLayoutSize(scrollOffset)), scrollContainerBox.size());
    }

    return parentGraphicsLayerRect;
}

void RenderLayerBacking::updateGeometry(const RenderLayer* compositedAncestor)
{
    ASSERT(!m_owningLayer.normalFlowListDirty());
    ASSERT(!m_owningLayer.zOrderListsDirty());
    ASSERT(!m_owningLayer.descendantDependentFlagsAreDirty());
    ASSERT(!renderer().view().needsLayout());

    const RenderStyle& style = renderer().style();
    const auto deviceScaleFactor = this->deviceScaleFactor();

    bool isRunningAcceleratedTransformAnimation = false;
    if (auto styleable = Styleable::fromRenderer(renderer()))
        isRunningAcceleratedTransformAnimation = styleable->isRunningAcceleratedTransformAnimation();

    updateTransform(style);
    updateOpacity(style);
    updateFilters(style);
#if ENABLE(FILTERS_LEVEL_2)
    updateBackdropFilters(style);
#endif
#if ENABLE(CSS_COMPOSITING)
    updateBlendMode(style);
#endif
    updateContentsScalingFilters(style);

    ASSERT(compositedAncestor == m_owningLayer.ancestorCompositingLayer());
    LayoutRect parentGraphicsLayerRect = computeParentGraphicsLayerRect(compositedAncestor);

    if (m_ancestorClippingStack)
        updateClippingStackLayerGeometry(*m_ancestorClippingStack, compositedAncestor, parentGraphicsLayerRect);

    LayoutRect primaryGraphicsLayerRect = computePrimaryGraphicsLayerRect(compositedAncestor, parentGraphicsLayerRect);

    ComputedOffsets compositedBoundsOffset(m_owningLayer, compositedAncestor, compositedBounds(), parentGraphicsLayerRect, primaryGraphicsLayerRect);
    ComputedOffsets rendererOffset(m_owningLayer, compositedAncestor, { }, parentGraphicsLayerRect, primaryGraphicsLayerRect);

    m_compositedBoundsOffsetFromGraphicsLayer = compositedBoundsOffset.fromPrimaryGraphicsLayer();

    auto primaryLayerPosition = primaryGraphicsLayerRect.location();

    // FIXME: reflections should force transform-style to be flat in the style: https://bugs.webkit.org/show_bug.cgi?id=106959
    bool preserves3D = style.preserves3D() && !renderer().hasReflection();

    if (m_viewportAnchorLayer) {
        m_viewportAnchorLayer->setPosition(primaryLayerPosition);
        primaryLayerPosition = { };
    }

    if (m_contentsContainmentLayer) {
        m_contentsContainmentLayer->setPreserves3D(preserves3D);
        m_contentsContainmentLayer->setPosition(primaryLayerPosition);
        primaryLayerPosition = { };
        // Use the same size as m_graphicsLayer so transforms behave correctly.
        m_contentsContainmentLayer->setSize(primaryGraphicsLayerRect.size());
    }

    auto computeAnimationExtent = [&] () -> std::optional<FloatRect> {
        LayoutRect animatedBounds;
        if (isRunningAcceleratedTransformAnimation && m_owningLayer.getOverlapBoundsIncludingChildrenAccountingForTransformAnimations(animatedBounds, RenderLayer::IncludeCompositedDescendants))
            return FloatRect(animatedBounds);
        return { };
    };
    m_graphicsLayer->setAnimationExtent(computeAnimationExtent());
    m_graphicsLayer->setPreserves3D(preserves3D);
    m_graphicsLayer->setBackfaceVisibility(style.backfaceVisibility() == BackfaceVisibility::Visible);

    m_graphicsLayer->setPosition(primaryLayerPosition);
    m_graphicsLayer->setSize(primaryGraphicsLayerRect.size());

#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
    // FIXME: Take ancestry into account and remove unnecessary structural layers.
    m_graphicsLayer->setIsSeparated(style.usedTransformStyle3D() == TransformStyle3D::Optimized3D);
#endif

    // Compute renderer offset from primary graphics layer. Note that primaryGraphicsLayerRect is in parentGraphicsLayer's coordinate system which is not necessarily
    // the same as the ancestor graphics layer.
    OffsetFromRenderer primaryGraphicsLayerOffsetFromRenderer;
    LayoutSize oldSubpixelOffsetFromRenderer = m_subpixelOffsetFromRenderer;
    primaryGraphicsLayerOffsetFromRenderer = computeOffsetFromRenderer(-rendererOffset.fromPrimaryGraphicsLayer(), deviceScaleFactor);
    m_subpixelOffsetFromRenderer = primaryGraphicsLayerOffsetFromRenderer.m_subpixelOffset;
    m_hasSubpixelRounding = !m_subpixelOffsetFromRenderer.isZero() || compositedBounds().size() != primaryGraphicsLayerRect.size();

    if (primaryGraphicsLayerOffsetFromRenderer.m_devicePixelOffset != m_graphicsLayer->offsetFromRenderer())
        m_graphicsLayer->setOffsetFromRenderer(primaryGraphicsLayerOffsetFromRenderer.m_devicePixelOffset);

    // If we have a layer that clips children, position it.
    LayoutRect clippingBox;
    if (auto* clipLayer = clippingLayer()) {
        // clipLayer is the m_childContainmentLayer.
        clippingBox = clippingLayerBox(renderer());
        // Clipping layer is parented in the primary graphics layer.
        LayoutSize clipBoxOffsetFromGraphicsLayer = toLayoutSize(clippingBox.location()) + rendererOffset.fromPrimaryGraphicsLayer();
        SnappedRectInfo snappedClippingGraphicsLayer = snappedGraphicsLayer(clipBoxOffsetFromGraphicsLayer, clippingBox.size(), renderer());
        clipLayer->setPosition(snappedClippingGraphicsLayer.m_snappedRect.location());
        clipLayer->setSize(snappedClippingGraphicsLayer.m_snappedRect.size());
        clipLayer->setOffsetFromRenderer(toLayoutSize(clippingBox.location() - snappedClippingGraphicsLayer.m_snapDelta));

        auto computeMasksToBoundsRect = [&] {
            if ((renderer().style().clipPath() || renderer().style().hasBorderRadius())) {
                auto contentsClippingRect = FloatRoundedRect(renderer().style().getRoundedInnerBorderFor(m_owningLayer.rendererBorderBoxRect()));
                contentsClippingRect.move(LayoutSize(-clipLayer->offsetFromRenderer()));
                return contentsClippingRect;
            }

            return FloatRoundedRect { FloatRect { { }, snappedClippingGraphicsLayer.m_snappedRect.size() } };
        };

        clipLayer->setContentsClippingRect(computeMasksToBoundsRect());
    }

    if (m_maskLayer)
        updateMaskingLayerGeometry();

    updateChildrenTransformAndAnchorPoint(primaryGraphicsLayerRect, rendererOffset.fromParentGraphicsLayer());

    if (m_owningLayer.reflectionLayer() && m_owningLayer.reflectionLayer()->isComposited()) {
        auto* reflectionBacking = m_owningLayer.reflectionLayer()->backing();
        reflectionBacking->updateGeometry(&m_owningLayer);
        
        // The reflection layer has the bounds of m_owningLayer.reflectionLayer(),
        // but the reflected layer is the bounds of this layer, so we need to position it appropriately.
        FloatRect layerBounds = this->compositedBounds();
        FloatRect reflectionLayerBounds = reflectionBacking->compositedBounds();
        reflectionBacking->graphicsLayer()->setReplicatedLayerPosition(FloatPoint(layerBounds.location() - reflectionLayerBounds.location()));
    }

    if (m_scrollContainerLayer) {
        ASSERT(m_scrolledContentsLayer);
        LayoutRect scrollContainerBox = scrollContainerLayerBox(downcast<RenderBox>(renderer()));
        LayoutRect parentLayerBounds = clippingLayer() ? scrollContainerBox : compositedBounds();

        // FIXME: need to do some pixel snapping here.
        m_scrollContainerLayer->setPosition(FloatPoint(scrollContainerBox.location() - parentLayerBounds.location()));
        m_scrollContainerLayer->setSize(roundedIntSize(LayoutSize(scrollContainerBox.width(), scrollContainerBox.height())));

        auto* scrollableArea = m_owningLayer.scrollableArea();
        ASSERT(scrollableArea);

        ScrollOffset scrollOffset = scrollableArea->scrollOffset();
        updateScrollOffset(scrollOffset);

        FloatSize oldScrollingLayerOffset = m_scrollContainerLayer->offsetFromRenderer();
        m_scrollContainerLayer->setOffsetFromRenderer(toFloatSize(scrollContainerBox.location()));

        bool paddingBoxOffsetChanged = oldScrollingLayerOffset != m_scrollContainerLayer->offsetFromRenderer();

        IntSize scrollSize;
        if (scrollableArea)
            scrollSize = IntSize(scrollableArea->scrollWidth(), scrollableArea->scrollHeight());
        if (scrollSize != m_scrolledContentsLayer->size() || paddingBoxOffsetChanged)
            m_scrolledContentsLayer->setNeedsDisplay();

        m_scrolledContentsLayer->setSize(scrollSize);
        m_scrolledContentsLayer->setScrollOffset(scrollOffset, GraphicsLayer::DontSetNeedsDisplay);
        m_scrolledContentsLayer->setOffsetFromRenderer(toLayoutSize(scrollContainerBox.location()), GraphicsLayer::DontSetNeedsDisplay);
        
        adjustTiledBackingCoverage();
    }

    if (m_overflowControlsContainer) {
        LayoutRect overflowControlsBox = overflowControlsHostLayerRect(downcast<RenderBox>(renderer()));
        LayoutSize boxOffsetFromGraphicsLayer = toLayoutSize(overflowControlsBox.location()) + rendererOffset.fromPrimaryGraphicsLayer();
        SnappedRectInfo snappedBoxInfo = snappedGraphicsLayer(boxOffsetFromGraphicsLayer, overflowControlsBox.size(), renderer());

        m_overflowControlsContainer->setPosition(snappedBoxInfo.m_snappedRect.location());
        m_overflowControlsContainer->setSize(snappedBoxInfo.m_snappedRect.size());
        m_overflowControlsContainer->setMasksToBounds(true);
    }

    if (m_foregroundLayer) {
        FloatSize foregroundSize;
        FloatSize foregroundOffset;
        GraphicsLayer::ShouldSetNeedsDisplay needsDisplayOnOffsetChange = GraphicsLayer::SetNeedsDisplay;
        if (m_scrolledContentsLayer) {
            foregroundSize = m_scrolledContentsLayer->size();
            foregroundOffset = m_scrolledContentsLayer->offsetFromRenderer() - toLayoutSize(m_scrolledContentsLayer->scrollOffset());
            needsDisplayOnOffsetChange = GraphicsLayer::DontSetNeedsDisplay;
        } else if (hasClippingLayer()) {
            // If we have a clipping layer (which clips descendants), then the foreground layer is a child of it,
            // so that it gets correctly sorted with children. In that case, position relative to the clipping layer.
            foregroundSize = FloatSize(clippingBox.size());
            foregroundOffset = toFloatSize(clippingBox.location());
        } else {
            foregroundSize = primaryGraphicsLayerRect.size();
            foregroundOffset = m_graphicsLayer->offsetFromRenderer();
        }

        m_foregroundLayer->setPosition({ });
        m_foregroundLayer->setSize(foregroundSize);
        m_foregroundLayer->setOffsetFromRenderer(foregroundOffset, needsDisplayOnOffsetChange);
    }

    if (m_backgroundLayer) {
        FloatPoint backgroundPosition;
        FloatSize backgroundSize = primaryGraphicsLayerRect.size();
        if (backgroundLayerPaintsFixedRootBackground()) {
            const FrameView& frameView = renderer().view().frameView();
            backgroundPosition = frameView.scrollPositionForFixedPosition();
            backgroundSize = frameView.layoutSize();
        } else {
            auto boundingBox = renderer().objectBoundingBox();
            backgroundPosition = boundingBox.location();
            backgroundSize = boundingBox.size();
        }
        m_backgroundLayer->setPosition(backgroundPosition);
        m_backgroundLayer->setSize(backgroundSize);
        m_backgroundLayer->setOffsetFromRenderer(m_graphicsLayer->offsetFromRenderer());
    }

    // If this layer was created just for clipping or to apply perspective, it doesn't need its own backing store.
    LayoutRect ancestorCompositedBounds = compositedAncestor ? compositedAncestor->backing()->compositedBounds() : LayoutRect();
    setRequiresOwnBackingStore(compositor().requiresOwnBackingStore(m_owningLayer, compositedAncestor,
        LayoutRect(toLayoutPoint(compositedBoundsOffset.fromParentGraphicsLayer()), compositedBounds().size()), ancestorCompositedBounds));
#if ENABLE(FILTERS_LEVEL_2)
    updateBackdropFiltersGeometry();
#endif
    updateAfterWidgetResize();

    positionOverflowControlsLayers();

    if (subpixelOffsetFromRendererChanged(oldSubpixelOffsetFromRenderer, m_subpixelOffsetFromRenderer, deviceScaleFactor) && canIssueSetNeedsDisplay())
        setContentsNeedDisplay();

#if ENABLE(MODEL_ELEMENT)
    if (is<RenderModel>(renderer()))
        downcast<HTMLModelElement>(renderer().element())->sizeMayHaveChanged();
#endif
}

void RenderLayerBacking::adjustOverflowControlsPositionRelativeToAncestor(const RenderLayer& ancestorLayer)
{
    ASSERT(m_overflowControlsContainer);
    ASSERT(ancestorLayer.isComposited());
    auto ancestorBacking = ancestorLayer.backing();
    if (!ancestorBacking)
        return;

    LayoutRect parentGraphicsLayerRect = computeParentGraphicsLayerRect(&ancestorLayer);
    LayoutRect primaryGraphicsLayerRect = computePrimaryGraphicsLayerRect(&ancestorLayer, parentGraphicsLayerRect);

    auto overflowControlsRect = overflowControlsHostLayerRect(downcast<RenderBox>(renderer()));

    if (overflowControlsHostLayerAncestorClippingStack())
        updateClippingStackLayerGeometry(*m_overflowControlsHostLayerAncestorClippingStack, &ancestorLayer, parentGraphicsLayerRect);

    ComputedOffsets rendererOffset(m_owningLayer, &ancestorLayer, { }, parentGraphicsLayerRect, primaryGraphicsLayerRect);

    LayoutSize boxOffsetFromGraphicsLayer = toLayoutSize(overflowControlsRect.location()) + rendererOffset.fromParentGraphicsLayer();
    SnappedRectInfo snappedBoxInfo = snappedGraphicsLayer(boxOffsetFromGraphicsLayer, overflowControlsRect.size(), renderer());

    m_overflowControlsContainer->setPosition(snappedBoxInfo.m_snappedRect.location());
    m_overflowControlsContainer->setSize(snappedBoxInfo.m_snappedRect.size());
}

void RenderLayerBacking::setLocationOfScrolledContents(ScrollOffset scrollOffset, ScrollingLayerPositionAction setOrSync)
{
    if (setOrSync == ScrollingLayerPositionAction::Sync)
        m_scrollContainerLayer->syncBoundsOrigin(scrollOffset);
    else
        m_scrollContainerLayer->setBoundsOrigin(scrollOffset);
}

void RenderLayerBacking::updateScrollOffset(ScrollOffset scrollOffset)
{
    auto* scrollableArea = m_owningLayer.scrollableArea();
    ASSERT(scrollableArea);

    if (scrollableArea->currentScrollType() == ScrollType::User) {
        // If scrolling is happening externally, we don't want to touch the layer bounds origin here because that will cause jitter.
        setLocationOfScrolledContents(scrollOffset, ScrollingLayerPositionAction::Sync);
        scrollableArea->setRequiresScrollPositionReconciliation(true);
    } else {
        // Note that we implement the contents offset via the bounds origin on this layer, rather than a position on the sublayer.
        setLocationOfScrolledContents(scrollOffset, ScrollingLayerPositionAction::Set);
        scrollableArea->setRequiresScrollPositionReconciliation(false);
    }

    ASSERT(m_scrolledContentsLayer->position().isZero());
}

void RenderLayerBacking::updateAfterDescendants()
{
    // FIXME: this potentially duplicates work we did in updateConfiguration().
    PaintedContentsInfo contentsInfo(*this);

    if (!m_owningLayer.isRenderViewLayer()) {
        bool didUpdateContentsRect = false;
        updateDirectlyCompositedBoxDecorations(contentsInfo, didUpdateContentsRect);
        if (!didUpdateContentsRect && m_graphicsLayer->usesContentsLayer())
            resetContentsRect();
    }

    updateDrawsContent(contentsInfo);

    if (!m_isMainFrameRenderViewLayer && !m_isFrameLayerWithTiledBacking && !m_requiresBackgroundLayer) {
        // For non-root layers, background is always painted by the primary graphics layer.
        ASSERT(!m_backgroundLayer);
        m_graphicsLayer->setContentsOpaque(!m_hasSubpixelRounding && m_owningLayer.backgroundIsKnownToBeOpaqueInRect(compositedBounds()));
    }

    bool isSkippedContent = renderer().isSkippedContent();
    m_graphicsLayer->setContentsVisible(!isSkippedContent && (m_owningLayer.hasVisibleContent() || hasVisibleNonCompositedDescendants()));
    if (m_scrollContainerLayer) {
        m_scrollContainerLayer->setContentsVisible(renderer().style().visibility() == Visibility::Visible);

        bool userInteractive = renderer().visibleToHitTesting();
        m_scrollContainerLayer->setUserInteractionEnabled(userInteractive);
        if (m_layerForHorizontalScrollbar)
            m_layerForHorizontalScrollbar->setUserInteractionEnabled(userInteractive);
        if (m_layerForVerticalScrollbar)
            m_layerForVerticalScrollbar->setUserInteractionEnabled(userInteractive);
        if (m_layerForScrollCorner)
            m_layerForScrollCorner->setUserInteractionEnabled(userInteractive);
    }

#if USE(OWNING_LAYER_BEAR_TRAP)
    RELEASE_ASSERT_WITH_MESSAGE(m_owningLayerBearTrap == BEAR_TRAP_VALUE, "RenderLayerBacking::updateAfterDescendants(): m_owningLayerBearTrap caught the bear (55699292)");
    RELEASE_ASSERT_WITH_MESSAGE(&m_owningLayer, "RenderLayerBacking::updateAfterDescendants(): m_owningLayer is null (55699292)");
#endif
}

// FIXME: Avoid repaints when clip path changes.
void RenderLayerBacking::updateMaskingLayerGeometry()
{
    m_maskLayer->setSize(m_graphicsLayer->size());
    m_maskLayer->setPosition(FloatPoint());
    m_maskLayer->setOffsetFromRenderer(m_graphicsLayer->offsetFromRenderer());
    
    if (!m_maskLayer->drawsContent()) {
        if (renderer().hasClipPath()) {
            ASSERT(renderer().style().clipPath()->type() != PathOperation::Reference);

            // FIXME: Use correct reference box for inlines: https://bugs.webkit.org/show_bug.cgi?id=129047, https://github.com/w3c/csswg-drafts/issues/6383
            LayoutRect boundingBox = m_owningLayer.boundingBox(&m_owningLayer);
            LayoutRect referenceBoxForClippedInline = LayoutRect(snapRectToDevicePixelsIfNeeded(boundingBox, renderer()));
            LayoutSize offset = LayoutSize(snapSizeToDevicePixel(-m_subpixelOffsetFromRenderer, LayoutPoint(), deviceScaleFactor()));
            auto [clipPath, windRule] = m_owningLayer.computeClipPath(offset, referenceBoxForClippedInline);

            FloatSize pathOffset = m_maskLayer->offsetFromRenderer();
            if (!pathOffset.isZero())
                clipPath.translate(-pathOffset);
            
            m_maskLayer->setShapeLayerPath(clipPath);
            m_maskLayer->setShapeLayerWindRule(windRule);
        }
    }
}

void RenderLayerBacking::updateDirectlyCompositedBoxDecorations(PaintedContentsInfo& contentsInfo, bool& didUpdateContentsRect)
{
    if (!m_owningLayer.hasVisibleContent())
        return;

    // The order of operations here matters, since the last valid type of contents needs
    // to also update the contentsRect.
    updateDirectlyCompositedBackgroundColor(contentsInfo, didUpdateContentsRect);
    updateDirectlyCompositedBackgroundImage(contentsInfo, didUpdateContentsRect);
}

void RenderLayerBacking::updateInternalHierarchy()
{
    // m_foregroundLayer has to be inserted in the correct order with child layers,
    // so it's not inserted here.
    GraphicsLayer* lastClippingLayer = nullptr;
    if (m_ancestorClippingStack) {
        connectClippingStackLayers(*m_ancestorClippingStack);
        lastClippingLayer = m_ancestorClippingStack->lastLayer();
    }

    constexpr size_t maxOrderedLayers = 6;
    Vector<GraphicsLayer*, maxOrderedLayers> orderedLayers;

    if (m_transformFlatteningLayer)
        orderedLayers.append(m_transformFlatteningLayer.get());

    if (lastClippingLayer)
        orderedLayers.append(lastClippingLayer);

    if (m_viewportAnchorLayer)
        orderedLayers.append(m_viewportAnchorLayer.get());

    if (m_contentsContainmentLayer) {
        m_contentsContainmentLayer->removeAllChildren();

        ASSERT(m_backgroundLayer);
        m_contentsContainmentLayer->addChild(*m_backgroundLayer);

        // The loop below will add a second child to the m_contentsContainmentLayer.
        orderedLayers.append(m_contentsContainmentLayer.get());
    }

    orderedLayers.append(m_graphicsLayer.get());

    if (m_childContainmentLayer)
        orderedLayers.append(m_childContainmentLayer.get());

    if (m_scrollContainerLayer)
        orderedLayers.append(m_scrollContainerLayer.get());

    GraphicsLayer* previousLayer = nullptr;
    for (auto* layer : orderedLayers) {
        if (previousLayer)
            previousLayer->addChild(*layer);

        previousLayer = layer;
    }

    // The clip for child layers does not include space for overflow controls, so they exist as
    // siblings of the clipping layer if we have one. Normal children of this layer are set as
    // children of the clipping layer.
    if (m_overflowControlsContainer) {
        if (m_layerForHorizontalScrollbar)
            m_overflowControlsContainer->addChild(*m_layerForHorizontalScrollbar);

        if (m_layerForVerticalScrollbar)
            m_overflowControlsContainer->addChild(*m_layerForVerticalScrollbar);

        if (m_layerForScrollCorner)
            m_overflowControlsContainer->addChild(*m_layerForScrollCorner);

        // m_overflowControlsContainer may get reparented later.
        m_graphicsLayer->addChild(*m_overflowControlsContainer);
    }
}

void RenderLayerBacking::updateContentsRects()
{
    m_graphicsLayer->setContentsRect(snapRectToDevicePixelsIfNeeded(contentsBox(), renderer()));
    
    if (is<RenderReplaced>(renderer())) {
        FloatRoundedRect contentsClippingRect = downcast<RenderReplaced>(renderer()).roundedContentBoxRect().pixelSnappedRoundedRectForPainting(deviceScaleFactor());
        contentsClippingRect.move(contentOffsetInCompositingLayer());
        m_graphicsLayer->setContentsClippingRect(contentsClippingRect);
    }
}

void RenderLayerBacking::resetContentsRect()
{
    updateContentsRects();
    m_graphicsLayer->setContentsTileSize(IntSize());
    m_graphicsLayer->setContentsTilePhase(IntSize());
}

void RenderLayerBacking::updateDrawsContent()
{
    PaintedContentsInfo contentsInfo(*this);
    updateDrawsContent(contentsInfo);
}

void RenderLayerBacking::updateDrawsContent(PaintedContentsInfo& contentsInfo)
{
    if (m_scrollContainerLayer) {
        // We don't have to consider overflow controls, because we know that the scrollbars are drawn elsewhere.
        // m_graphicsLayer only needs backing store if the non-scrolling parts (background, outlines, borders, shadows etc) need to paint.
        // m_scrollContainerLayer never has backing store.
        // m_scrolledContentsLayer only needs backing store if the scrolled contents need to paint.
        bool hasNonScrollingPaintedContent = m_owningLayer.hasVisibleContent() && m_owningLayer.hasVisibleBoxDecorationsOrBackground();
        m_graphicsLayer->setDrawsContent(hasNonScrollingPaintedContent);

        bool hasScrollingPaintedContent = hasBackingSharingLayers() || (m_owningLayer.hasVisibleContent() && (renderer().hasBackground() || contentsInfo.paintsContent()));
        m_scrolledContentsLayer->setDrawsContent(hasScrollingPaintedContent);
        return;
    }

    bool hasPaintedContent = containsPaintedContent(contentsInfo);

    // FIXME: we could refine this to only allocate backing for one of these layers if possible.
    m_graphicsLayer->setDrawsContent(hasPaintedContent);
    if (m_foregroundLayer)
        m_foregroundLayer->setDrawsContent(hasPaintedContent);

    if (m_backgroundLayer)
        m_backgroundLayer->setDrawsContent(m_backgroundLayerPaintsFixedRootBackground ? hasPaintedContent : contentsInfo.paintsBoxDecorations());
}

#if ENABLE(ASYNC_SCROLLING)
bool RenderLayerBacking::maintainsEventRegion() const
{
    if (!m_owningLayer.page().scrollingCoordinator())
        return false;

    if (paintsIntoCompositedAncestor())
        return false;

    if (renderer().view().needsEventRegionUpdateForNonCompositedFrame())
        return true;

#if ENABLE(TOUCH_ACTION_REGIONS)
    if (renderer().document().mayHaveElementsWithNonAutoTouchAction())
        return true;
#endif
#if ENABLE(EDITABLE_REGION)
    LOG_WITH_STREAM(EventRegions, stream << "RenderLayerBacking: " << this << " maintainsEventRegion - mayHaveEditableElements: " << renderer().document().mayHaveEditableElements() << " shouldBuildEditableRegion: " << renderer().page().shouldBuildEditableRegion());
    if (renderer().document().mayHaveEditableElements() && renderer().page().shouldBuildEditableRegion())
        return true;
#endif
#if ENABLE(WHEEL_EVENT_REGIONS)
    if (renderer().document().hasWheelEventHandlers())
        return true;
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    if (renderer().page().shouldBuildInteractionRegions())
        return true;
#endif

    if (m_owningLayer.isRenderViewLayer())
        return false;

    auto& settings = renderer().settings();
    if (!settings.asyncFrameScrollingEnabled() && !settings.asyncOverflowScrollingEnabled())
        return false;

    if (!m_owningLayer.page().scrollingCoordinator()->hasSubscrollers())
        return false;

    return true;
}

void RenderLayerBacking::updateEventRegion()
{
    LOG_WITH_STREAM(EventRegions, stream << m_owningLayer << " " << this << " updateEventRegion (needs update: " << needsEventRegionUpdate() << ", maintainsEventRegion: " << maintainsEventRegion() << ")");

    if (!maintainsEventRegion())
        return;

    if (!needsEventRegionUpdate())
        return;

    TraceScope scope(ComputeEventRegionsStart, ComputeEventRegionsEnd);

    auto visibleToHitTesting = renderer().visibleToHitTesting();

    auto setEventRegionToLayerBounds = [&](GraphicsLayer* graphicsLayer) {
        if (!graphicsLayer)
            return;

        EventRegion eventRegion;
        auto eventRegionContext = eventRegion.makeContext();
        if (visibleToHitTesting)
            eventRegionContext.unite(enclosingIntRect(FloatRect({ }, graphicsLayer->size())), renderer(), renderer().style());

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        eventRegionContext.copyInteractionRegionsToEventRegion();
#endif
        graphicsLayer->setEventRegion(WTFMove(eventRegion));
    };

    auto updateEventRegionForLayer = [&](GraphicsLayer& graphicsLayer) {
        NullGraphicsContext nullContext;
        EventRegion eventRegion;
#if ENABLE(EDITABLE_REGION)
        if (renderer().page().shouldBuildEditableRegion())
            eventRegion.ensureEditableRegion();
#endif
        auto eventRegionContext = eventRegion.makeContext();
        auto layerOffset = graphicsLayer.scrollOffset() - roundedIntSize(graphicsLayer.offsetFromRenderer());
        auto layerBounds = enclosingIntRect(FloatRect(-layerOffset, graphicsLayer.size()));

        if (visibleToHitTesting) {
            if (&graphicsLayer == m_scrolledContentsLayer) {
                // Initialize scrolled contents layer with layer-sized event region as it can all used for scrolling.
                // This avoids generating unnecessarily complex event regions. We still need to to do the paint to capture touch-action regions.
                eventRegionContext.unite(layerBounds, renderer(), renderer().style());
            }
        }

        if (m_owningLayer.isRenderViewLayer() && (&graphicsLayer == m_graphicsLayer || &graphicsLayer == m_foregroundLayer)) {
            // Event handlers on the root cover the entire layer.
            eventRegionContext.unite(layerBounds, renderer(), renderer().style());
        }

        auto dirtyRect = enclosingIntRect(FloatRect(FloatPoint(graphicsLayer.offsetFromRenderer()), graphicsLayer.size()));
        paintIntoLayer(&graphicsLayer, nullContext, dirtyRect, { }, &eventRegionContext);

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        eventRegionContext.copyInteractionRegionsToEventRegion();
#endif
        eventRegion.translate(toIntSize(layerOffset));
        graphicsLayer.setEventRegion(WTFMove(eventRegion));
    };

    updateEventRegionForLayer(*m_graphicsLayer);

    setEventRegionToLayerBounds(m_scrollContainerLayer.get());
    setEventRegionToLayerBounds(m_layerForHorizontalScrollbar.get());
    setEventRegionToLayerBounds(m_layerForVerticalScrollbar.get());
    setEventRegionToLayerBounds(m_layerForScrollCorner.get());

    if (m_scrolledContentsLayer)
        updateEventRegionForLayer(*m_scrolledContentsLayer);

    if (m_foregroundLayer)
        updateEventRegionForLayer(*m_foregroundLayer);

    renderer().view().setNeedsEventRegionUpdateForNonCompositedFrame(false);
    
    setNeedsEventRegionUpdate(false);
}
#endif

bool RenderLayerBacking::updateAncestorClippingStack(Vector<CompositedClipData>&& clippingData)
{
    if (!m_ancestorClippingStack && clippingData.isEmpty())
        return false;

    auto* scrollingCoordinator = m_owningLayer.page().scrollingCoordinator();

    if (m_ancestorClippingStack && clippingData.isEmpty()) {
        m_ancestorClippingStack->clear(scrollingCoordinator);
        m_ancestorClippingStack = nullptr;
        
        if (m_overflowControlsHostLayerAncestorClippingStack) {
            m_overflowControlsHostLayerAncestorClippingStack->clear(scrollingCoordinator);
            m_overflowControlsHostLayerAncestorClippingStack = nullptr;
        }
        return true;
    }
    
    if (!m_ancestorClippingStack) {
        m_ancestorClippingStack = makeUnique<LayerAncestorClippingStack>(WTFMove(clippingData));
        LOG_WITH_STREAM(Compositing, stream << "layer " << &m_owningLayer << " ancestorClippingStack " << *m_ancestorClippingStack);
        return true;
    }
    
    if (m_ancestorClippingStack->equalToClipData(clippingData)) {
        LOG_WITH_STREAM(Compositing, stream << "layer " << &m_owningLayer << " ancestorClippingStack " << *m_ancestorClippingStack);
        return false;
    }
    
    m_ancestorClippingStack->updateWithClipData(scrollingCoordinator, WTFMove(clippingData));
    LOG_WITH_STREAM(Compositing, stream << "layer " << &m_owningLayer << " ancestorClippingStack " << *m_ancestorClippingStack);
    if (m_overflowControlsHostLayerAncestorClippingStack)
        m_overflowControlsHostLayerAncestorClippingStack->updateWithClipData(scrollingCoordinator, WTFMove(clippingData));
    return true;
}

void RenderLayerBacking::ensureOverflowControlsHostLayerAncestorClippingStack(const RenderLayer* compositedAncestor)
{
    auto* scrollingCoordinator = m_owningLayer.page().scrollingCoordinator();
    auto clippingData = m_ancestorClippingStack->compositedClipData();

    if (m_overflowControlsHostLayerAncestorClippingStack)
        m_overflowControlsHostLayerAncestorClippingStack->updateWithClipData(scrollingCoordinator, WTFMove(clippingData));
    else
        m_overflowControlsHostLayerAncestorClippingStack = makeUnique<LayerAncestorClippingStack>(WTFMove(clippingData));

    ensureClippingStackLayers(*m_overflowControlsHostLayerAncestorClippingStack);

    LayoutRect parentGraphicsLayerRect = computeParentGraphicsLayerRect(compositedAncestor);
    updateClippingStackLayerGeometry(*m_overflowControlsHostLayerAncestorClippingStack, compositedAncestor, parentGraphicsLayerRect);

    connectClippingStackLayers(*m_overflowControlsHostLayerAncestorClippingStack);
}

void RenderLayerBacking::ensureClippingStackLayers(LayerAncestorClippingStack& clippingStack)
{
    for (auto& entry : clippingStack.stack()) {
        if (!entry.clippingLayer) {
            entry.clippingLayer = createGraphicsLayer(entry.clipData.isOverflowScroll ? "clip for scroller"_s : "ancestor clipping"_s);
            entry.clippingLayer->setMasksToBounds(true);
            entry.clippingLayer->setPaintingPhase({ });
        }

        if (entry.clipData.isOverflowScroll) {
            if (!entry.scrollingLayer)
                entry.scrollingLayer = createGraphicsLayer("scrolling proxy"_s);
        } else if (entry.scrollingLayer)
            GraphicsLayer::unparentAndClear(entry.scrollingLayer);
    }
}

void RenderLayerBacking::removeClippingStackLayers(LayerAncestorClippingStack& clippingStack)
{
    for (auto& entry : clippingStack.stack()) {
        GraphicsLayer::unparentAndClear(entry.clippingLayer);
        GraphicsLayer::unparentAndClear(entry.scrollingLayer);
    }
}

void RenderLayerBacking::connectClippingStackLayers(LayerAncestorClippingStack& clippingStack)
{
    auto connectEntryLayers = [](LayerAncestorClippingStack::ClippingStackEntry& entry) {
        if (entry.scrollingLayer)
            entry.clippingLayer->setChildren({ Ref { *entry.scrollingLayer } });
    };

    auto& clippingEntryStack = clippingStack.stack();
    for (unsigned i = 0; i < clippingEntryStack.size() - 1; ++i) {
        auto& entry = clippingEntryStack.at(i);
        connectEntryLayers(entry);

        auto* entryParentForSublayers = entry.parentForSublayers();
        auto* childLayer = clippingEntryStack.at(i + 1).childForSuperlayers();
        entryParentForSublayers->setChildren({ Ref { *childLayer } });
    }

    connectEntryLayers(clippingEntryStack.last());
    clippingEntryStack.last().parentForSublayers()->removeAllChildren();
}

void RenderLayerBacking::updateClippingStackLayerGeometry(LayerAncestorClippingStack& clippingStack, const RenderLayer* compositedAncestor, LayoutRect& parentGraphicsLayerRect)
{
    // All clipRects in the stack are computed relative to m_owningLayer, so convert them back to compositedAncestor.
    auto offsetFromCompositedAncestor = toLayoutSize(m_owningLayer.convertToLayerCoords(compositedAncestor, { }, RenderLayer::AdjustForColumns));
    LayoutRect lastClipLayerRect = parentGraphicsLayerRect;

    auto deviceScaleFactor = this->deviceScaleFactor();
    for (auto& entry : clippingStack.stack()) {
        auto roundedClipRect = entry.clipData.clipRect;
        auto clipRect = roundedClipRect.rect();
        LayoutSize clippingOffset = computeOffsetFromAncestorGraphicsLayer(compositedAncestor, clipRect.location() + offsetFromCompositedAncestor, deviceScaleFactor);
        LayoutRect snappedClippingLayerRect = snappedGraphicsLayer(clippingOffset, clipRect.size(), renderer()).m_snappedRect;
        
        auto clippingLayerPosition = toLayoutPoint(snappedClippingLayerRect.location() - lastClipLayerRect.location());
        entry.clippingLayer->setPosition(clippingLayerPosition);
        entry.clippingLayer->setSize(snappedClippingLayerRect.size());

        clipRect.setLocation({ });
        roundedClipRect.setRect(clipRect);
        entry.clippingLayer->setContentsClippingRect(FloatRoundedRect(roundedClipRect));
        entry.clippingLayer->setContentsRectClipsDescendants(true);

        lastClipLayerRect = snappedClippingLayerRect;

        if (entry.clipData.isOverflowScroll) {
            ScrollOffset scrollOffset;
            if (auto* scrollableArea = entry.clipData.clippingLayer ? entry.clipData.clippingLayer->scrollableArea() : nullptr)
                scrollOffset = scrollableArea->scrollOffset();

            // scrollingLayer size and position are always 0,0.
            entry.scrollingLayer->setBoundsOrigin(scrollOffset);
            lastClipLayerRect.moveBy(-scrollOffset);
        }
    }

    parentGraphicsLayerRect = lastClipLayerRect;
}

// Return true if the layer changed.
bool RenderLayerBacking::updateAncestorClipping(bool needsAncestorClip, const RenderLayer* compositingAncestor)
{
    bool layersChanged = false;

    if (needsAncestorClip) {
        if (compositor().updateAncestorClippingStack(m_owningLayer, compositingAncestor)) {
            if (m_ancestorClippingStack)
                ensureClippingStackLayers(*m_ancestorClippingStack);

            layersChanged = true;
        }
    } else if (m_ancestorClippingStack) {
        removeClippingStackLayers(*m_ancestorClippingStack);
        m_ancestorClippingStack = nullptr;
        
        if (m_overflowControlsHostLayerAncestorClippingStack) {
            removeClippingStackLayers(*m_overflowControlsHostLayerAncestorClippingStack);
            m_overflowControlsHostLayerAncestorClippingStack = nullptr;
        }
        
        layersChanged = true;
    }
    
    return layersChanged;
}

// Return true if the layer changed.
bool RenderLayerBacking::updateDescendantClippingLayer(bool needsDescendantClip)
{
    bool layersChanged = false;

    if (needsDescendantClip) {
        if (!m_childContainmentLayer && !m_isFrameLayerWithTiledBacking) {
            m_childContainmentLayer = createGraphicsLayer("child clipping"_s);
            m_childContainmentLayer->setMasksToBounds(true);
            m_childContainmentLayer->setContentsRectClipsDescendants(true);
            layersChanged = true;
        }
    } else if (hasClippingLayer()) {
        willDestroyLayer(m_childContainmentLayer.get());
        GraphicsLayer::unparentAndClear(m_childContainmentLayer);
        layersChanged = true;
    }
    
    return layersChanged;
}

bool RenderLayerBacking::needsRepaintOnCompositedScroll() const
{
    if (!hasScrollingLayer())
        return false;

    if (renderer().style().hasAnyLocalBackground())
        return true;

    if (auto scrollingCoordinator = m_owningLayer.page().scrollingCoordinator())
        return scrollingCoordinator->hasSynchronousScrollingReasons(m_scrollingNodeID);

    return false;
}

void RenderLayerBacking::setBackgroundLayerPaintsFixedRootBackground(bool backgroundLayerPaintsFixedRootBackground)
{
    if (backgroundLayerPaintsFixedRootBackground == m_backgroundLayerPaintsFixedRootBackground)
        return;

    m_backgroundLayerPaintsFixedRootBackground = backgroundLayerPaintsFixedRootBackground;

    if (m_backgroundLayerPaintsFixedRootBackground) {
        ASSERT(m_isFrameLayerWithTiledBacking);
        renderer().view().frameView().removeSlowRepaintObject(*renderer().view().rendererForRootBackground());
    }
}

void RenderLayerBacking::setRequiresBackgroundLayer(bool requiresBackgroundLayer)
{
    if (requiresBackgroundLayer == m_requiresBackgroundLayer)
        return;

    m_requiresBackgroundLayer = requiresBackgroundLayer;
    m_owningLayer.setNeedsCompositingConfigurationUpdate();
}

bool RenderLayerBacking::requiresLayerForScrollbar(Scrollbar* scrollbar) const
{
    return scrollbar && (scrollbar->isOverlayScrollbar()
#if !PLATFORM(IOS_FAMILY) // FIXME: This should be an #if ENABLE(): webkit.org/b/210460
        || renderer().settings().asyncOverflowScrollingEnabled()
#endif
        );
}

bool RenderLayerBacking::requiresHorizontalScrollbarLayer() const
{
    if (auto* scrollableArea = m_owningLayer.scrollableArea())
        return requiresLayerForScrollbar(scrollableArea->horizontalScrollbar());
    return false;
}

bool RenderLayerBacking::requiresVerticalScrollbarLayer() const
{
    if (auto* scrollableArea = m_owningLayer.scrollableArea())
        return requiresLayerForScrollbar(scrollableArea->verticalScrollbar());
    return false;
}

bool RenderLayerBacking::requiresScrollCornerLayer() const
{
    if (!is<RenderBox>(m_owningLayer.renderer()))
        return false;

    auto* scrollableArea = m_owningLayer.scrollableArea();
    if (!scrollableArea)
        return false;

    auto cornerRect = scrollableArea->overflowControlsRects().scrollCornerOrResizerRect();
    if (cornerRect.isEmpty())
        return false;

    auto verticalScrollbar = scrollableArea->verticalScrollbar();
    auto scrollbar = verticalScrollbar ? verticalScrollbar : scrollableArea->horizontalScrollbar();
    return requiresLayerForScrollbar(scrollbar);
}

bool RenderLayerBacking::updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer)
{
    auto createOrDestroyLayer = [&](RefPtr<GraphicsLayer>& layer, bool needLayer, bool drawsContent, ASCIILiteral layerName) {
        if (needLayer == !!layer)
            return false;

        if (needLayer) {
            layer = createGraphicsLayer(layerName);
            if (drawsContent) {
                layer->setAllowsBackingStoreDetaching(false);
                layer->setAllowsTiling(false);
            } else {
                layer->setPaintingPhase({ });
                layer->setDrawsContent(false);
            }
        } else {
            willDestroyLayer(layer.get());
            GraphicsLayer::unparentAndClear(layer);
        }
        return true;
    };

    bool layersChanged = createOrDestroyLayer(m_overflowControlsContainer, needsHorizontalScrollbarLayer || needsVerticalScrollbarLayer || needsScrollCornerLayer, false, "overflow controls container"_s);

    bool horizontalScrollbarLayerChanged = createOrDestroyLayer(m_layerForHorizontalScrollbar, needsHorizontalScrollbarLayer, true, "horizontal scrollbar"_s);
    layersChanged |= horizontalScrollbarLayerChanged;

    bool verticalScrollbarLayerChanged = createOrDestroyLayer(m_layerForVerticalScrollbar, needsVerticalScrollbarLayer, true, "vertical scrollbar"_s);
    layersChanged |= verticalScrollbarLayerChanged;

    layersChanged |= createOrDestroyLayer(m_layerForScrollCorner, needsScrollCornerLayer, true, "scroll corner"_s);

    if (auto* scrollingCoordinator = m_owningLayer.page().scrollingCoordinator()) {
        if (auto* scrollableArea = m_owningLayer.scrollableArea()) {
            if (horizontalScrollbarLayerChanged)
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(*scrollableArea, ScrollbarOrientation::Horizontal);
            if (verticalScrollbarLayerChanged)
                scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(*scrollableArea, ScrollbarOrientation::Vertical);
        }
    }

    return layersChanged;
}

void RenderLayerBacking::positionOverflowControlsLayers()
{
    auto* scrollableArea = m_owningLayer.scrollableArea();
    if (!scrollableArea || !scrollableArea->hasScrollbars())
        return;
    // FIXME: Should do device-pixel snapping.
    auto box = renderBox();
    auto borderBox = snappedIntRect(box->borderBoxRect());

    // m_overflowControlsContainer is positioned using the paddingBoxRectIncludingScrollbar.
    auto paddingBox = snappedIntRect(box->paddingBoxRectIncludingScrollbar());
    auto paddingBoxInset = paddingBox.location() - borderBox.location();

    auto positionScrollbarLayer = [](GraphicsLayer& layer, const IntRect& scrollbarRect, IntSize paddingBoxInset) {
        layer.setPosition(scrollbarRect.location() - paddingBoxInset);
        layer.setSize(scrollbarRect.size());
        if (layer.usesContentsLayer()) {
            IntRect barRect = IntRect(IntPoint(), scrollbarRect.size());
            layer.setContentsRect(barRect);
            layer.setContentsClippingRect(FloatRoundedRect(barRect));
        }
    };

    // These rects are relative to the borderBoxRect.
    auto rects = scrollableArea->overflowControlsRects();
    if (auto* layer = layerForHorizontalScrollbar()) {
        positionScrollbarLayer(*layer, rects.horizontalScrollbar, paddingBoxInset);
        layer->setDrawsContent(scrollableArea->horizontalScrollbar() && !layer->usesContentsLayer());
    }

    if (auto* layer = layerForVerticalScrollbar()) {
        positionScrollbarLayer(*layer, rects.verticalScrollbar, paddingBoxInset);
        layer->setDrawsContent(scrollableArea->verticalScrollbar() && !layer->usesContentsLayer());
    }

    if (auto* layer = layerForScrollCorner()) {
        auto cornerRect = rects.scrollCornerOrResizerRect();
        layer->setPosition(cornerRect.location() - paddingBoxInset);
        layer->setSize(cornerRect.size());
        layer->setDrawsContent(!cornerRect.isEmpty());
    }
}

static bool ancestorLayerIsDOMParent(RenderLayer& layer, const RenderLayer* compositingAncestor)
{
    if (!compositingAncestor)
        return false;
    if (!layer.renderer().element() || !layer.renderer().element()->parentElement())
        return false;
    return compositingAncestor->renderer().element() == layer.renderer().element()->parentElement();
}

static bool ancestorLayerWillCombineTransform(const RenderLayer* compositingAncestor)
{
    if (!compositingAncestor)
        return false;
    return compositingAncestor->preserves3D() || compositingAncestor->hasPerspective();
}

bool RenderLayerBacking::updateTransformFlatteningLayer(const RenderLayer* compositingAncestor)
{
    bool needsFlatteningLayer = false;
    // If our parent layer has preserve-3d or perspective, and it's not our DOM parent, then we need a flattening layer to block that from being applied in 3d.
    if (useCSS3DTransformInteroperability() && ancestorLayerWillCombineTransform(compositingAncestor) && !ancestorLayerIsDOMParent(m_owningLayer, compositingAncestor))
        needsFlatteningLayer = true;

    bool layerChanged = false;
    if (needsFlatteningLayer) {
        if (!m_transformFlatteningLayer) {
            m_transformFlatteningLayer = createGraphicsLayer(makeString(m_owningLayer.name(), " (3d flattening)"));
            layerChanged = true;
        }
    } else if (m_transformFlatteningLayer) {
        willDestroyLayer(m_transformFlatteningLayer.get());
        GraphicsLayer::unparentAndClear(m_transformFlatteningLayer);
        layerChanged = true;
    }

    return layerChanged;
}

bool RenderLayerBacking::updateViewportConstrainedAnchorLayer(bool needsAnchorLayer)
{
    bool layerChanged = false;
    if (needsAnchorLayer) {
        if (!m_viewportAnchorLayer) {
            String layerName = m_owningLayer.name() + " (anchor)";
            m_viewportAnchorLayer = createGraphicsLayer(layerName, GraphicsLayer::Type::Structural);
            layerChanged = true;
        }
    } else if (m_viewportAnchorLayer) {
        willDestroyLayer(m_viewportAnchorLayer.get());
        GraphicsLayer::unparentAndClear(m_viewportAnchorLayer);
        layerChanged = true;
    }

    return layerChanged;
}

bool RenderLayerBacking::updateForegroundLayer(bool needsForegroundLayer)
{
    bool layerChanged = false;
    if (needsForegroundLayer) {
        if (!m_foregroundLayer) {
            String layerName = m_owningLayer.name() + " (foreground)";
            m_foregroundLayer = createGraphicsLayer(layerName);
            m_foregroundLayer->setDrawsContent(true);
            layerChanged = true;
        }
    } else if (m_foregroundLayer) {
        willDestroyLayer(m_foregroundLayer.get());
        GraphicsLayer::unparentAndClear(m_foregroundLayer);
        layerChanged = true;
    }

    return layerChanged;
}

bool RenderLayerBacking::updateBackgroundLayer(bool needsBackgroundLayer)
{
    bool layerChanged = false;
    if (needsBackgroundLayer) {
        if (!m_backgroundLayer) {
            String layerName = m_owningLayer.name() + " (background)";
            m_backgroundLayer = createGraphicsLayer(layerName);
            m_backgroundLayer->setDrawsContent(true);
            m_backgroundLayer->setAnchorPoint(FloatPoint3D());
            layerChanged = true;
        }
        
        if (!m_contentsContainmentLayer) {
            String layerName = m_owningLayer.name() + " (contents containment)";
            m_contentsContainmentLayer = createGraphicsLayer(layerName);
            m_contentsContainmentLayer->setAppliesPageScale(true);
            m_graphicsLayer->setAppliesPageScale(false);
            layerChanged = true;
        }
    } else {
        if (m_backgroundLayer) {
            willDestroyLayer(m_backgroundLayer.get());
            GraphicsLayer::unparentAndClear(m_backgroundLayer);
            layerChanged = true;
        }
        if (m_contentsContainmentLayer) {
            willDestroyLayer(m_contentsContainmentLayer.get());
            GraphicsLayer::unparentAndClear(m_contentsContainmentLayer);
            layerChanged = true;
            m_graphicsLayer->setAppliesPageScale(true);
        }
    }

    return layerChanged;
}

// Masking layer is used for masks or clip-path.
bool RenderLayerBacking::updateMaskingLayer(bool hasMask, bool hasClipPath)
{
    bool layerChanged = false;
    if (hasMask || hasClipPath) {
        OptionSet<GraphicsLayerPaintingPhase> maskPhases;
        if (hasMask)
            maskPhases = GraphicsLayerPaintingPhase::Mask;
        
        if (hasClipPath) {
            // If we have a mask, we need to paint the combined clip-path and mask into the mask layer.
            if (hasMask || renderer().style().clipPath()->type() == PathOperation::Reference || !GraphicsLayer::supportsLayerType(GraphicsLayer::Type::Shape))
                maskPhases.add(GraphicsLayerPaintingPhase::ClipPath);
        }

        bool paintsContent = !maskPhases.isEmpty();
        GraphicsLayer::Type requiredLayerType = paintsContent ? GraphicsLayer::Type::Normal : GraphicsLayer::Type::Shape;
        if (m_maskLayer && m_maskLayer->type() != requiredLayerType) {
            m_graphicsLayer->setMaskLayer(nullptr);
            willDestroyLayer(m_maskLayer.get());
            GraphicsLayer::clear(m_maskLayer);
        }

        if (!m_maskLayer) {
            m_maskLayer = createGraphicsLayer("mask"_s, requiredLayerType);
            layerChanged = true;
            m_graphicsLayer->setMaskLayer(m_maskLayer.copyRef());
            // We need a geometry update to size the new mask layer.
            m_owningLayer.setNeedsCompositingGeometryUpdate();
        }
        m_maskLayer->setDrawsContent(paintsContent);
        m_maskLayer->setPaintingPhase(maskPhases);
    } else if (m_maskLayer) {
        m_graphicsLayer->setMaskLayer(nullptr);
        willDestroyLayer(m_maskLayer.get());
        GraphicsLayer::clear(m_maskLayer);
        layerChanged = true;
    }

    return layerChanged;
}

bool RenderLayerBacking::updateScrollingLayers(bool needsScrollingLayers)
{
    if (needsScrollingLayers == !!m_scrollContainerLayer)
        return false;

    if (!m_scrollContainerLayer) {
        // Outer layer which corresponds with the scroll view. This never paints content.
        m_scrollContainerLayer = createGraphicsLayer("scroll container"_s, GraphicsLayer::Type::ScrollContainer);
        m_scrollContainerLayer->setPaintingPhase({ });
        m_scrollContainerLayer->setDrawsContent(false);
        m_scrollContainerLayer->setMasksToBounds(true);

        // Inner layer which renders the content that scrolls.
        m_scrolledContentsLayer = createGraphicsLayer("scrolled contents"_s, GraphicsLayer::Type::ScrolledContents);
        m_scrolledContentsLayer->setDrawsContent(true);
        m_scrolledContentsLayer->setAnchorPoint({ });
        m_scrollContainerLayer->addChild(*m_scrolledContentsLayer);
    } else {
        compositor().willRemoveScrollingLayerWithBacking(m_owningLayer, *this);

        willDestroyLayer(m_scrollContainerLayer.get());
        willDestroyLayer(m_scrolledContentsLayer.get());
        
        GraphicsLayer::unparentAndClear(m_scrollContainerLayer);
        GraphicsLayer::unparentAndClear(m_scrolledContentsLayer);
    }

    if (m_scrollContainerLayer)
        compositor().didAddScrollingLayer(m_owningLayer);
    
    return true;
}

void RenderLayerBacking::detachFromScrollingCoordinator(OptionSet<ScrollCoordinationRole> roles)
{
    if (!m_scrollingNodeID && !m_ancestorClippingStack && !m_frameHostingNodeID && !m_viewportConstrainedNodeID && !m_positioningNodeID)
        return;

    auto* scrollingCoordinator = m_owningLayer.page().scrollingCoordinator();
    if (!scrollingCoordinator)
        return;

    if (roles.contains(ScrollCoordinationRole::Scrolling) && m_scrollingNodeID) {
        LOG_WITH_STREAM(Compositing, stream << "Detaching Scrolling node " << m_scrollingNodeID);
        scrollingCoordinator->unparentChildrenAndDestroyNode(m_scrollingNodeID);
        m_scrollingNodeID = 0;
        
#if ENABLE(SCROLLING_THREAD)
        if (m_scrollContainerLayer)
            m_scrollContainerLayer->setScrollingNodeID(0);
        if (m_layerForHorizontalScrollbar)
            m_layerForHorizontalScrollbar->setScrollingNodeID(0);
        if (m_layerForVerticalScrollbar)
            m_layerForVerticalScrollbar->setScrollingNodeID(0);
        if (m_layerForScrollCorner)
            m_layerForScrollCorner->setScrollingNodeID(0);
#endif
    }

    if (roles.contains(ScrollCoordinationRole::ScrollingProxy) && m_ancestorClippingStack) {
        m_ancestorClippingStack->detachFromScrollingCoordinator(*scrollingCoordinator);
        LOG_WITH_STREAM(Compositing, stream << "Detaching nodes in ancestor clipping stack");
    }

    if (roles.contains(ScrollCoordinationRole::FrameHosting) && m_frameHostingNodeID) {
        LOG_WITH_STREAM(Compositing, stream << "Detaching FrameHosting node " << m_frameHostingNodeID);
        scrollingCoordinator->unparentChildrenAndDestroyNode(m_frameHostingNodeID);
        m_frameHostingNodeID = 0;
    }

    if (roles.contains(ScrollCoordinationRole::ViewportConstrained) && m_viewportConstrainedNodeID) {
        LOG_WITH_STREAM(Compositing, stream << "Detaching ViewportConstrained node " << m_viewportConstrainedNodeID);
        scrollingCoordinator->unparentChildrenAndDestroyNode(m_viewportConstrainedNodeID);
        m_viewportConstrainedNodeID = 0;
    }

    if (roles.contains(ScrollCoordinationRole::Positioning) && m_positioningNodeID) {
        LOG_WITH_STREAM(Compositing, stream << "Detaching Positioned node " << m_positioningNodeID);
        scrollingCoordinator->unparentChildrenAndDestroyNode(m_positioningNodeID);
        m_positioningNodeID = 0;
#if ENABLE(SCROLLING_THREAD)
        m_graphicsLayer->setScrollingNodeID(0);
#endif
    }
}

void RenderLayerBacking::setScrollingNodeIDForRole(ScrollingNodeID nodeID, ScrollCoordinationRole role)
{
    switch (role) {
    case ScrollCoordinationRole::Scrolling:
        m_scrollingNodeID = nodeID;
#if ENABLE(SCROLLING_THREAD)
        if (m_scrollContainerLayer)
            m_scrollContainerLayer->setScrollingNodeID(m_scrollingNodeID);

        if (m_layerForHorizontalScrollbar)
            m_layerForHorizontalScrollbar->setScrollingNodeID(m_scrollingNodeID);
        if (m_layerForVerticalScrollbar)
            m_layerForVerticalScrollbar->setScrollingNodeID(m_scrollingNodeID);
        if (m_layerForScrollCorner)
            m_layerForScrollCorner->setScrollingNodeID(m_scrollingNodeID);
#endif
        break;
    case ScrollCoordinationRole::ScrollingProxy:
        // These nodeIDs are stored in m_ancestorClippingStack.
        ASSERT_NOT_REACHED();
        break;
    case ScrollCoordinationRole::FrameHosting:
        m_frameHostingNodeID = nodeID;
        break;
    case ScrollCoordinationRole::ViewportConstrained:
        m_viewportConstrainedNodeID = nodeID;
        break;
    case ScrollCoordinationRole::Positioning:
        m_positioningNodeID = nodeID;
#if ENABLE(SCROLLING_THREAD)
        m_graphicsLayer->setScrollingNodeID(m_positioningNodeID);
#endif
        break;
    }
}

ScrollingNodeID RenderLayerBacking::scrollingNodeIDForChildren() const
{
    if (m_frameHostingNodeID)
        return m_frameHostingNodeID;

    if (m_scrollingNodeID)
        return m_scrollingNodeID;

    if (m_viewportConstrainedNodeID)
        return m_viewportConstrainedNodeID;

    if (m_ancestorClippingStack) {
        if (auto lastOverflowScrollProxyNode = m_ancestorClippingStack->lastOverflowScrollProxyNodeID())
            return lastOverflowScrollProxyNode;
    }

    return m_positioningNodeID;
}

float RenderLayerBacking::compositingOpacity(float rendererOpacity) const
{
    float finalOpacity = rendererOpacity;

    for (auto* curr = m_owningLayer.stackingContext(); curr; curr = curr->stackingContext()) {
        // If we found a compositing layer, we want to compute opacity
        // relative to it. So we can break here.
        if (curr->isComposited())
            break;

        finalOpacity *= curr->renderer().opacity();
    }

    return finalOpacity;
}

// FIXME: Code is duplicated in RenderLayer. Also, we should probably not consider filters a box decoration here.
static inline bool hasVisibleBoxDecorations(const RenderStyle& style)
{
    return style.hasVisibleBorder() || style.hasBorderRadius() || style.hasOutline() || style.hasEffectiveAppearance() || style.boxShadow() || style.hasFilter();
}

static bool canDirectlyCompositeBackgroundBackgroundImage(const RenderStyle& style)
{
    if (!GraphicsLayer::supportsContentsTiling())
        return false;

    auto& fillLayer = style.backgroundLayers();
    if (fillLayer.next())
        return false;

    if (!fillLayer.imagesAreLoaded())
        return false;

    if (fillLayer.attachment() != FillAttachment::ScrollBackground)
        return false;

    // FIXME: Allow color+image compositing when it makes sense.
    // For now bailing out.
    if (style.visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor).isVisible())
        return false;

    // FIXME: support gradients with isGeneratedImage.
    auto* styleImage = fillLayer.image();
    if (!styleImage->hasCachedImage())
        return false;

    auto* image = styleImage->cachedImage()->image();
    if (!image->isBitmapImage())
        return false;

    return true;
}

static bool hasPaintedBoxDecorationsOrBackgroundImage(const RenderStyle& style)
{
    if (hasVisibleBoxDecorations(style))
        return true;

    if (!style.hasBackgroundImage())
        return false;

    return !canDirectlyCompositeBackgroundBackgroundImage(style);
}

static inline bool hasPerspectiveOrPreserves3D(const RenderStyle& style)
{
    return style.hasPerspective() || style.preserves3D();
}

Color RenderLayerBacking::rendererBackgroundColor() const
{
    RenderElement* backgroundRenderer = nullptr;
    if (renderer().isDocumentElementRenderer())
        backgroundRenderer = renderer().view().rendererForRootBackground();
    
    if (!backgroundRenderer)
        backgroundRenderer = &renderer();

    return backgroundRenderer->style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
}

void RenderLayerBacking::updateDirectlyCompositedBackgroundColor(PaintedContentsInfo& contentsInfo, bool& didUpdateContentsRect)
{
    if (m_backgroundLayer && !m_backgroundLayerPaintsFixedRootBackground && !contentsInfo.paintsBoxDecorations()) {
        m_graphicsLayer->setContentsToSolidColor(Color());
        m_backgroundLayer->setContentsToSolidColor(rendererBackgroundColor());

        FloatRect contentsRect = backgroundBoxForSimpleContainerPainting();
        // NOTE: This is currently only used by RenderFullScreen, which we want to be
        // big enough to hide overflow areas of the root.
        contentsRect.inflate(contentsRect.size());
        m_backgroundLayer->setContentsRect(contentsRect);
        m_backgroundLayer->setContentsClippingRect(FloatRoundedRect(contentsRect));
        return;
    }

    if (!contentsInfo.isSimpleContainer() || (is<RenderBox>(renderer()) && !BackgroundPainter::paintsOwnBackground(*renderBox()))) {
        m_graphicsLayer->setContentsToSolidColor(Color());
        return;
    }

    Color backgroundColor = rendererBackgroundColor();

    // An unset (invalid) color will remove the solid color.
    m_graphicsLayer->setContentsToSolidColor(backgroundColor);
    FloatRect contentsRect = backgroundBoxForSimpleContainerPainting();
    m_graphicsLayer->setContentsRect(contentsRect);
    m_graphicsLayer->setContentsClippingRect(FloatRoundedRect(contentsRect));
    didUpdateContentsRect = true;
}

void RenderLayerBacking::updateDirectlyCompositedBackgroundImage(PaintedContentsInfo& contentsInfo, bool& didUpdateContentsRect)
{
    if (!GraphicsLayer::supportsContentsTiling())
        return;

    if (contentsInfo.isDirectlyCompositedImage())
        return;

    auto& style = renderer().style();
    if (!contentsInfo.isSimpleContainer() || !style.hasBackgroundImage()) {
        m_graphicsLayer->setContentsToImage(nullptr);
        return;
    }

    auto backgroundBox = LayoutRect { backgroundBoxForSimpleContainerPainting() };
    // FIXME: Absolute paint location is required here.
    auto geometry = BackgroundPainter::calculateBackgroundImageGeometry(*renderBox(), renderBox(), style.backgroundLayers(), { }, backgroundBox);

    m_graphicsLayer->setContentsTileSize(geometry.tileSize);
    m_graphicsLayer->setContentsTilePhase(geometry.phase);
    m_graphicsLayer->setContentsRect(geometry.destinationRect);
    m_graphicsLayer->setContentsClippingRect(FloatRoundedRect(geometry.destinationRect));
    m_graphicsLayer->setContentsToImage(style.backgroundLayers().image()->cachedImage()->image());

    didUpdateContentsRect = true;
}

void RenderLayerBacking::updateRootLayerConfiguration()
{
    if (!m_isFrameLayerWithTiledBacking)
        return;

    Color backgroundColor;
    bool viewIsTransparent = compositor().viewHasTransparentBackground(&backgroundColor);

    if (m_backgroundLayerPaintsFixedRootBackground && m_backgroundLayer) {
        if (m_isMainFrameRenderViewLayer) {
            m_backgroundLayer->setBackgroundColor(backgroundColor);
            m_backgroundLayer->setContentsOpaque(!viewIsTransparent);
        }

        m_graphicsLayer->setBackgroundColor(Color());
        m_graphicsLayer->setContentsOpaque(false);
    } else if (m_isMainFrameRenderViewLayer) {
        m_graphicsLayer->setBackgroundColor(backgroundColor);
        m_graphicsLayer->setContentsOpaque(!viewIsTransparent);
    }
}

void RenderLayerBacking::updatePaintingPhases()
{
    // Phases for m_maskLayer are set elsewhere.
    OptionSet<GraphicsLayerPaintingPhase> primaryLayerPhases = { GraphicsLayerPaintingPhase::Background, GraphicsLayerPaintingPhase::Foreground };
    
    if (m_foregroundLayer) {
        OptionSet<GraphicsLayerPaintingPhase> foregroundLayerPhases { GraphicsLayerPaintingPhase::Foreground };
        
        if (m_scrolledContentsLayer)
            foregroundLayerPhases.add(GraphicsLayerPaintingPhase::OverflowContents);

        m_foregroundLayer->setPaintingPhase(foregroundLayerPhases);
        primaryLayerPhases.remove(GraphicsLayerPaintingPhase::Foreground);
    }

    if (m_backgroundLayer) {
        m_backgroundLayer->setPaintingPhase(GraphicsLayerPaintingPhase::Background);
        primaryLayerPhases.remove(GraphicsLayerPaintingPhase::Background);
    }

    if (m_scrolledContentsLayer) {
        OptionSet<GraphicsLayerPaintingPhase> scrolledContentLayerPhases = { GraphicsLayerPaintingPhase::OverflowContents, GraphicsLayerPaintingPhase::CompositedScroll };
        if (!m_foregroundLayer)
            scrolledContentLayerPhases.add(GraphicsLayerPaintingPhase::Foreground);
        m_scrolledContentsLayer->setPaintingPhase(scrolledContentLayerPhases);

        primaryLayerPhases.remove(GraphicsLayerPaintingPhase::Foreground);
        primaryLayerPhases.add(GraphicsLayerPaintingPhase::CompositedScroll);
    }

    m_graphicsLayer->setPaintingPhase(primaryLayerPhases);
}

static bool supportsDirectlyCompositedBoxDecorations(const RenderLayerModelObject& renderer)
{
    const RenderStyle& style = renderer.style();
    if (renderer.hasClip())
        return false;

    if (hasPaintedBoxDecorationsOrBackgroundImage(style))
        return false;

    // FIXME: We can't create a directly composited background if this
    // layer will have children that intersect with the background layer.
    // A better solution might be to introduce a flattening layer if
    // we do direct box decoration composition.
    // https://bugs.webkit.org/show_bug.cgi?id=119461
    if (hasPerspectiveOrPreserves3D(style))
        return false;

    return true;
}

bool RenderLayerBacking::paintsBoxDecorations() const
{
    if (!m_owningLayer.hasVisibleBoxDecorations())
        return false;

    return !supportsDirectlyCompositedBoxDecorations(renderer());
}

bool RenderLayerBacking::paintsContent(RenderLayer::PaintedContentRequest& request) const
{
    m_owningLayer.updateDescendantDependentFlags();

    bool paintsContent = false;

    if (m_owningLayer.hasVisibleContent() && m_owningLayer.hasNonEmptyChildRenderers(request))
        paintsContent = true;

    if (request.isSatisfied())
        return paintsContent;

    if (isPaintDestinationForDescendantLayers(request))
        paintsContent = true;

    if (request.isSatisfied())
        return paintsContent;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGModelObject>(m_owningLayer.renderer())) {
        // FIXME: [LBSE] Eventually cache if we're part of a RenderSVGHiddenContainer subtree to avoid tree walks.
        // FIXME: [LBSE] Eventually refine the logic to end up with a narrower set of conditions (webkit.org/b/243417).
        paintsContent = m_owningLayer.hasVisibleContent() && !lineageOfType<RenderSVGHiddenContainer>(m_owningLayer.renderer()).first();
        request.setHasPaintedContent();
    }

    if (request.isSatisfied())
        return paintsContent;
#endif

    if (request.hasPaintedContent == RequestState::Unknown)
        request.hasPaintedContent = RequestState::False;

    return paintsContent;
}

static bool isCompositedPlugin(RenderObject& renderer)
{
    return is<RenderEmbeddedObject>(renderer) && downcast<RenderEmbeddedObject>(renderer).allowsAcceleratedCompositing();
}

// A "simple container layer" is a RenderLayer which has no visible content to render.
// It may have no children, or all its children may be themselves composited.
// This is a useful optimization, because it allows us to avoid allocating backing store.
bool RenderLayerBacking::isSimpleContainerCompositingLayer(PaintedContentsInfo& contentsInfo) const
{
    if (m_owningLayer.isRenderViewLayer())
        return false;

    if (hasBackingSharingLayers())
        return false;

    if (renderer().isRenderReplaced() && !isCompositedPlugin(renderer()))
        return false;

    if (renderer().isTextControl())
        return false;

    if (contentsInfo.paintsBoxDecorations() || contentsInfo.paintsContent())
        return false;

    if (renderer().style().backgroundClip() == FillBox::Text)
        return false;
    
    if (renderer().isDocumentElementRenderer() && m_owningLayer.isolatesCompositedBlending())
        return false;

    return true;
}

// Returning true stops the traversal.
enum class LayerTraversal { Continue, Stop };

static LayerTraversal traverseVisibleNonCompositedDescendantLayers(RenderLayer& parent, const Function<LayerTraversal(const RenderLayer&)>& layerFunc)
{
    // FIXME: We shouldn't be called with a stale z-order lists. See bug 85512.
    parent.updateLayerListsIfNeeded();

#if ASSERT_ENABLED
    LayerListMutationDetector mutationChecker(parent);
#endif

    for (auto* childLayer : parent.normalFlowLayers()) {
        if (compositedWithOwnBackingStore(*childLayer))
            continue;

        if (layerFunc(*childLayer) == LayerTraversal::Stop)
            return LayerTraversal::Stop;
        
        if (traverseVisibleNonCompositedDescendantLayers(*childLayer, layerFunc) == LayerTraversal::Stop)
            return LayerTraversal::Stop;
    }

    if (parent.isStackingContext() && !parent.hasVisibleDescendant())
        return LayerTraversal::Continue;

    // Use the m_hasCompositingDescendant bit to optimize?
    for (auto* childLayer : parent.negativeZOrderLayers()) {
        if (compositedWithOwnBackingStore(*childLayer))
            continue;

        if (layerFunc(*childLayer) == LayerTraversal::Stop)
            return LayerTraversal::Stop;

        if (traverseVisibleNonCompositedDescendantLayers(*childLayer, layerFunc) == LayerTraversal::Stop)
            return LayerTraversal::Stop;
    }

    for (auto* childLayer : parent.positiveZOrderLayers()) {
        if (compositedWithOwnBackingStore(*childLayer))
            continue;

        if (layerFunc(*childLayer) == LayerTraversal::Stop)
            return LayerTraversal::Stop;

        if (traverseVisibleNonCompositedDescendantLayers(*childLayer, layerFunc) == LayerTraversal::Stop)
            return LayerTraversal::Stop;
    }

    return LayerTraversal::Continue;
}

static std::optional<bool> intersectsWithAncestor(const RenderLayer& child, const RenderLayer& ancestor, const LayoutRect& ancestorCompositedBounds)
{
    // If any layers between child and ancestor are transformed, then adjusting the offset is
    // insufficient to convert coordinates into ancestor's coordinate space.
    for (auto* layer = &child; layer != &ancestor; layer = layer->parent()) {
        if (!layer->canUseOffsetFromAncestor())
            return std::nullopt;
    }

    auto overlap = child.boundingBox(&ancestor, child.offsetFromAncestor(&ancestor), RenderLayer::UseFragmentBoxesExcludingCompositing);
    return overlap.intersects(ancestorCompositedBounds);
}

// Conservative test for having no rendered children.
bool RenderLayerBacking::isPaintDestinationForDescendantLayers(RenderLayer::PaintedContentRequest& request) const
{
    bool hasPaintingDescendant = false;
    traverseVisibleNonCompositedDescendantLayers(m_owningLayer, [&hasPaintingDescendant, &request, this](const RenderLayer& layer) {
        RenderLayer::PaintedContentRequest localRequest;
        if (layer.isVisuallyNonEmpty(&localRequest)) {
            bool mayIntersect = intersectsWithAncestor(layer, m_owningLayer, compositedBounds()).value_or(true);
            if (mayIntersect) {
                hasPaintingDescendant = true;
                request.setHasPaintedContent();
            }
        }
        return (hasPaintingDescendant && request.isSatisfied()) ? LayerTraversal::Stop : LayerTraversal::Continue;
    });

    return hasPaintingDescendant;
}

bool RenderLayerBacking::hasVisibleNonCompositedDescendants() const
{
    bool hasVisibleDescendant = false;
    traverseVisibleNonCompositedDescendantLayers(m_owningLayer, [&hasVisibleDescendant](const RenderLayer& layer) {
        hasVisibleDescendant |= layer.hasVisibleContent();
        return hasVisibleDescendant ? LayerTraversal::Stop : LayerTraversal::Continue;
    });

    return hasVisibleDescendant;
}

bool RenderLayerBacking::containsPaintedContent(PaintedContentsInfo& contentsInfo) const
{
    if (contentsInfo.isSimpleContainer() || paintsIntoWindow() || paintsIntoCompositedAncestor() || m_artificiallyInflatedBounds || m_owningLayer.isReflection())
        return false;

    if (contentsInfo.isDirectlyCompositedImage())
        return false;

    if (auto styleable = Styleable::fromRenderer(renderer())) {
        if (!styleable->mayHaveNonZeroOpacity())
            return false;
    }

    // FIXME: we could optimize cases where the image, video or canvas is known to fill the border box entirely,
    // and set background color on the layer in that case, instead of allocating backing store and painting.
#if ENABLE(VIDEO)
    if (is<RenderVideo>(renderer()) && downcast<RenderVideo>(renderer()).shouldDisplayVideo())
        return m_owningLayer.hasVisibleBoxDecorationsOrBackground() || (!(downcast<RenderVideo>(renderer()).supportsAcceleratedRendering()) && m_requiresOwnBackingStore);
#endif

#if ENABLE(WEBGL) || ENABLE(OFFSCREEN_CANVAS)
    if (is<RenderHTMLCanvas>(renderer()) && canvasCompositingStrategy(renderer()) == CanvasAsLayerContents)
        return m_owningLayer.hasVisibleBoxDecorationsOrBackground();
#endif

    return true;
}

// An image can be directly compositing if it's the sole content of the layer, and has no box decorations
// that require painting. Direct compositing saves backing store.
bool RenderLayerBacking::isDirectlyCompositedImage() const
{
    if (!is<RenderImage>(renderer()) || m_owningLayer.hasVisibleBoxDecorationsOrBackground() || m_owningLayer.paintsWithFilters() || renderer().hasClip())
        return false;

#if ENABLE(VIDEO)
    if (is<RenderMedia>(renderer()))
        return false;
#endif

    auto& imageRenderer = downcast<RenderImage>(renderer());
    if (auto* cachedImage = imageRenderer.cachedImage()) {
        if (!cachedImage->hasImage())
            return false;

        auto* image = cachedImage->imageForRenderer(&imageRenderer);
        if (!is<BitmapImage>(image))
            return false;

        if (downcast<BitmapImage>(*image).orientationForCurrentFrame() != ImageOrientation::None)
            return false;

#if (PLATFORM(GTK) || PLATFORM(WPE))
        // GTK and WPE ports don't support rounded rect clipping at TextureMapper level, so they cannot
        // directly composite images that have border-radius propery. Draw them as non directly composited
        // content instead. See https://bugs.webkit.org/show_bug.cgi?id=174157.
        if (imageRenderer.style().hasBorderRadius())
            return false;
#endif

        return m_graphicsLayer->shouldDirectlyCompositeImage(image);
    }

    return false;
}

bool RenderLayerBacking::isUnscaledBitmapOnly() const
{
    if (!is<RenderImage>(renderer()) && !is<RenderHTMLCanvas>(renderer()))
        return false;

    if (m_owningLayer.hasVisibleBoxDecorationsOrBackground())
        return false;

    if (pageScaleFactor() < 1.0f)
        return false;

    auto contents = contentsBox();
    if (contents.location() != LayoutPoint(0, 0))
        return false;

    if (is<RenderImage>(renderer())) {
        auto& imageRenderer = downcast<RenderImage>(renderer());
        if (auto* cachedImage = imageRenderer.cachedImage()) {
            if (!cachedImage->hasImage())
                return false;

            auto* image = cachedImage->imageForRenderer(&imageRenderer);
            if (!is<BitmapImage>(image))
                return false;

            if (downcast<BitmapImage>(*image).orientationForCurrentFrame() != ImageOrientation::None)
                return false;

            return contents.size() == image->size();
        }
        return false;
    }

    if (renderer().style().imageRendering() == ImageRendering::CrispEdges || renderer().style().imageRendering() == ImageRendering::Pixelated)
        return false;

    auto& canvasRenderer = downcast<RenderHTMLCanvas>(renderer());
    if (snappedIntRect(contents).size() == canvasRenderer.canvasElement().size())
        return true;
    return false;
}

void RenderLayerBacking::contentChanged(ContentChangeType changeType)
{
    PaintedContentsInfo contentsInfo(*this);
    if (changeType == ImageChanged) {
        if (contentsInfo.isDirectlyCompositedImage()) {
            updateImageContents(contentsInfo);
            return;
        }
        if (contentsInfo.isUnscaledBitmapOnly()) {
            compositor().scheduleCompositingLayerUpdate();
            return;
        }
    }

    if (changeType == VideoChanged) {
        compositor().scheduleCompositingLayerUpdate();
        return;
    }

#if ENABLE(MODEL_ELEMENT)
    if (changeType == ModelChanged) {
        compositor().scheduleCompositingLayerUpdate();
        return;
    }
#endif

    if ((changeType == BackgroundImageChanged) && canDirectlyCompositeBackgroundBackgroundImage(renderer().style()))
        m_owningLayer.setNeedsCompositingConfigurationUpdate();

    if ((changeType == MaskImageChanged) && m_maskLayer)
        m_owningLayer.setNeedsCompositingConfigurationUpdate();

#if ENABLE(WEBGL) || ENABLE(OFFSCREEN_CANVAS)
    if ((changeType == CanvasChanged || changeType == CanvasPixelsChanged) && renderer().isCanvas() && canvasCompositingStrategy(renderer()) == CanvasAsLayerContents) {
        if (changeType == CanvasChanged)
            compositor().scheduleCompositingLayerUpdate();

        m_graphicsLayer->setContentsNeedsDisplay();
        return;
    }
#endif
}

void RenderLayerBacking::updateImageContents(PaintedContentsInfo& contentsInfo)
{
    auto& imageRenderer = downcast<RenderImage>(renderer());

    auto* cachedImage = imageRenderer.cachedImage();
    if (!cachedImage)
        return;

    auto* image = cachedImage->imageForRenderer(&imageRenderer);
    if (!image)
        return;

    // We have to wait until the image is fully loaded before setting it on the layer.
    if (!cachedImage->isLoaded())
        return;

    updateContentsRects();
    m_graphicsLayer->setContentsToImage(image);
    
    updateDrawsContent(contentsInfo);
    
    // Image animation is "lazy", in that it automatically stops unless someone is drawing
    // the image. So we have to kick the animation each time; this has the downside that the
    // image will keep animating, even if its layer is not visible.
    image->startAnimation();
}

// Return the offset from the top-left of this compositing layer at which the renderer's contents are painted.
LayoutSize RenderLayerBacking::contentOffsetInCompositingLayer() const
{
    return LayoutSize(-m_compositedBounds.x() + m_compositedBoundsOffsetFromGraphicsLayer.width(), -m_compositedBounds.y() + m_compositedBoundsOffsetFromGraphicsLayer.height());
}

LayoutRect RenderLayerBacking::contentsBox() const
{
    if (!is<RenderBox>(renderer()))
        return LayoutRect();

    auto& renderBox = downcast<RenderBox>(renderer());
    LayoutRect contentsRect;
#if ENABLE(VIDEO)
    if (is<RenderVideo>(renderBox))
        contentsRect = downcast<RenderVideo>(renderBox).videoBox();
    else
#endif

    if (is<RenderReplaced>(renderBox) && !is<RenderWidget>(renderBox)) {
        RenderReplaced& renderReplaced = downcast<RenderReplaced>(renderBox);
        contentsRect = renderReplaced.replacedContentRect();
    } else
        contentsRect = renderBox.contentBoxRect();

    contentsRect.move(contentOffsetInCompositingLayer());
    return contentsRect;
}

static LayoutRect backgroundRectForBox(const RenderBox& box)
{
    switch (box.style().backgroundClip()) {
    case FillBox::Border:
        return box.borderBoxRect();
    case FillBox::Padding:
        return box.paddingBoxRect();
    case FillBox::Content:
        return box.contentBoxRect();
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return { };
}

FloatRect RenderLayerBacking::backgroundBoxForSimpleContainerPainting() const
{
    if (!is<RenderBox>(renderer()))
        return FloatRect();

    LayoutRect backgroundBox = backgroundRectForBox(downcast<RenderBox>(renderer()));
    backgroundBox.move(contentOffsetInCompositingLayer());
    return snapRectToDevicePixels(backgroundBox, deviceScaleFactor());
}

GraphicsLayer* RenderLayerBacking::parentForSublayers() const
{
    if (m_scrolledContentsLayer)
        return m_scrolledContentsLayer.get();

    return m_childContainmentLayer ? m_childContainmentLayer.get() : m_graphicsLayer.get();
}

GraphicsLayer* RenderLayerBacking::childForSuperlayers() const
{
    if (m_transformFlatteningLayer)
        return m_transformFlatteningLayer.get();

    if (m_ancestorClippingStack)
        return m_ancestorClippingStack->firstLayer();

    if (m_viewportAnchorLayer)
        return m_viewportAnchorLayer.get();

    if (m_contentsContainmentLayer)
        return m_contentsContainmentLayer.get();
    
    return m_graphicsLayer.get();
}

LayoutSize RenderLayerBacking::offsetRelativeToRendererOriginForDescendantLayers() const
{
    if (m_scrolledContentsLayer)
        return toLayoutSize(scrollContainerLayerBox(downcast<RenderBox>(renderer())).location());

    if (hasClippingLayer())
        return toLayoutSize(clippingLayerBox(renderer()).location());

    return { };
}

bool RenderLayerBacking::paintsIntoWindow() const
{
#if USE(COORDINATED_GRAPHICS)
        return false;
#endif

    if (m_isFrameLayerWithTiledBacking)
        return false;

    if (m_owningLayer.isRenderViewLayer()) {
#if PLATFORM(IOS_FAMILY) || USE(COORDINATED_GRAPHICS)
        if (compositor().inForcedCompositingMode())
            return false;
#endif

        return compositor().rootLayerAttachment() != RenderLayerCompositor::RootLayerAttachedViaEnclosingFrame;
    }
    
    return false;
}

void RenderLayerBacking::setRequiresOwnBackingStore(bool requiresOwnBacking)
{
    if (requiresOwnBacking == m_requiresOwnBackingStore)
        return;

    m_requiresOwnBackingStore = requiresOwnBacking;

    // This affects the answer to paintsIntoCompositedAncestor(), which in turn affects
    // cached clip rects, so when it changes we have to clear clip rects on descendants.
    m_owningLayer.clearClipRectsIncludingDescendants(PaintingClipRects);
    m_owningLayer.computeRepaintRectsIncludingDescendants();

    compositor().repaintInCompositedAncestor(m_owningLayer, compositedBounds());
}

void RenderLayerBacking::setContentsNeedDisplay(GraphicsLayer::ShouldClipToLayer shouldClip)
{
    ASSERT(!paintsIntoCompositedAncestor());

    // Use the repaint as a trigger to re-evaluate direct compositing (which is never used on the root layer).
    if (!m_owningLayer.isRenderViewLayer())
        m_owningLayer.setNeedsCompositingConfigurationUpdate();

    m_owningLayer.invalidateEventRegion(RenderLayer::EventRegionInvalidationReason::Paint);

    auto& frameView = renderer().view().frameView();
    if (m_isMainFrameRenderViewLayer && frameView.isTrackingRepaints())
        frameView.addTrackedRepaintRect(owningLayer().absoluteBoundingBoxForPainting());

    if (m_graphicsLayer && m_graphicsLayer->drawsContent()) {
        // By default, setNeedsDisplay will clip to the size of the GraphicsLayer, which does not include margin tiles.
        // So if the TiledBacking has a margin that needs to be invalidated, we need to send in a rect to setNeedsDisplayInRect
        // that is large enough to include the margin. TiledBacking::bounds() includes the margin.
        auto* tiledBacking = this->tiledBacking();
        FloatRect rectToRepaint = tiledBacking ? tiledBacking->bounds() : FloatRect(FloatPoint(0, 0), m_graphicsLayer->size());
        m_graphicsLayer->setNeedsDisplayInRect(rectToRepaint, shouldClip);
    }

    if (m_foregroundLayer && m_foregroundLayer->drawsContent())
        m_foregroundLayer->setNeedsDisplay();

    if (m_backgroundLayer && m_backgroundLayer->drawsContent())
        m_backgroundLayer->setNeedsDisplay();

    if (m_maskLayer && m_maskLayer->drawsContent())
        m_maskLayer->setNeedsDisplay();

    if (m_scrolledContentsLayer && m_scrolledContentsLayer->drawsContent())
        m_scrolledContentsLayer->setNeedsDisplay();
}

// r is in the coordinate space of the layer's render object
void RenderLayerBacking::setContentsNeedDisplayInRect(const LayoutRect& r, GraphicsLayer::ShouldClipToLayer shouldClip)
{
    ASSERT(!paintsIntoCompositedAncestor());
    
    // Use the repaint as a trigger to re-evaluate direct compositing (which is never used on the root layer).
    if (!m_owningLayer.isRenderViewLayer())
        m_owningLayer.setNeedsCompositingConfigurationUpdate();

    m_owningLayer.invalidateEventRegion(RenderLayer::EventRegionInvalidationReason::Paint);

    FloatRect pixelSnappedRectForPainting = snapRectToDevicePixelsIfNeeded(r, renderer());
    auto& frameView = renderer().view().frameView();
    if (m_isMainFrameRenderViewLayer && frameView.isTrackingRepaints())
        frameView.addTrackedRepaintRect(pixelSnappedRectForPainting);

    if (m_graphicsLayer && m_graphicsLayer->drawsContent()) {
        FloatRect layerDirtyRect = pixelSnappedRectForPainting;
        layerDirtyRect.move(-m_graphicsLayer->offsetFromRenderer() - m_subpixelOffsetFromRenderer);
        m_graphicsLayer->setNeedsDisplayInRect(layerDirtyRect, shouldClip);
    }

    if (m_foregroundLayer && m_foregroundLayer->drawsContent()) {
        FloatRect layerDirtyRect = pixelSnappedRectForPainting;
        layerDirtyRect.move(-m_foregroundLayer->offsetFromRenderer() - m_subpixelOffsetFromRenderer);
        m_foregroundLayer->setNeedsDisplayInRect(layerDirtyRect, shouldClip);
    }

    // FIXME: need to split out repaints for the background.
    if (m_backgroundLayer && m_backgroundLayer->drawsContent()) {
        FloatRect layerDirtyRect = pixelSnappedRectForPainting;
        layerDirtyRect.move(-m_backgroundLayer->offsetFromRenderer() - m_subpixelOffsetFromRenderer);
        m_backgroundLayer->setNeedsDisplayInRect(layerDirtyRect, shouldClip);
    }

    if (m_maskLayer && m_maskLayer->drawsContent()) {
        FloatRect layerDirtyRect = pixelSnappedRectForPainting;
        layerDirtyRect.move(-m_maskLayer->offsetFromRenderer() - m_subpixelOffsetFromRenderer);
        m_maskLayer->setNeedsDisplayInRect(layerDirtyRect, shouldClip);
    }

    if (m_scrolledContentsLayer && m_scrolledContentsLayer->drawsContent()) {
        FloatRect layerDirtyRect = pixelSnappedRectForPainting;
        ScrollOffset scrollOffset;
        if (auto* scrollableArea = m_owningLayer.scrollableArea())
            scrollOffset = scrollableArea->scrollOffset();
        layerDirtyRect.move(-m_scrolledContentsLayer->offsetFromRenderer() + toLayoutSize(scrollOffset) - m_subpixelOffsetFromRenderer);
        m_scrolledContentsLayer->setNeedsDisplayInRect(layerDirtyRect, shouldClip);
    }
}

void RenderLayerBacking::paintIntoLayer(const GraphicsLayer* graphicsLayer, GraphicsContext& context,
    const IntRect& paintDirtyRect, // In the coords of rootLayer.
    OptionSet<PaintBehavior> paintBehavior, EventRegionContext* eventRegionContext)
{
#if USE(OWNING_LAYER_BEAR_TRAP)
    RELEASE_ASSERT_WITH_MESSAGE(m_owningLayerBearTrap == BEAR_TRAP_VALUE, "RenderLayerBacking::paintIntoLayer(): m_owningLayerBearTrap caught the bear (55699292)");
    RELEASE_ASSERT_WITH_MESSAGE(&m_owningLayer, "RenderLayerBacking::paintIntoLayer(): m_owningLayer is null (55699292)");
#endif

    if ((paintsIntoWindow() || paintsIntoCompositedAncestor()) && graphicsLayer->paintingPhase() != OptionSet<GraphicsLayerPaintingPhase>(GraphicsLayerPaintingPhase::ChildClippingMask)) {
#if !PLATFORM(IOS_FAMILY) && !OS(WINDOWS)
        // FIXME: Looks like the CALayer tree is out of sync with the GraphicsLayer heirarchy
        // when pages are restored from the BackForwardCache.
        // <rdar://problem/8712587> ASSERT: When Going Back to Page with Plugins in BackForwardCache
        ASSERT_NOT_REACHED();
#endif
        return;
    }

    auto paintFlags = paintFlagsForLayer(*graphicsLayer);

    if (eventRegionContext)
        paintFlags.add(RenderLayer::PaintLayerFlag::CollectingEventRegion);

    RenderObject::SetLayoutNeededForbiddenScope forbidSetNeedsLayout(renderer());

    auto paintOneLayer = [&](RenderLayer& layer, OptionSet<RenderLayer::PaintLayerFlag> paintFlags) {
        FrameView::PaintingState paintingState;
        if (!eventRegionContext) {
            InspectorInstrumentation::willPaint(layer.renderer());

            if (layer.isRenderViewLayer())
                renderer().view().frameView().willPaintContents(context, paintDirtyRect, paintingState);
        }

        RenderLayer::LayerPaintingInfo paintingInfo(&m_owningLayer, paintDirtyRect, paintBehavior, -m_subpixelOffsetFromRenderer);
        paintingInfo.eventRegionContext = eventRegionContext;

        if (&layer == &m_owningLayer) {
            layer.paintLayerContents(context, paintingInfo, paintFlags);

            auto* scrollableArea = layer.scrollableArea();
            if (scrollableArea && scrollableArea->containsDirtyOverlayScrollbars() && !eventRegionContext)
                layer.paintLayerContents(context, paintingInfo, paintFlags | RenderLayer::PaintLayerFlag::PaintingOverlayScrollbars);
        } else
            layer.paintLayerWithEffects(context, paintingInfo, paintFlags);

        if (!eventRegionContext) {
            if (layer.isRenderViewLayer())
                renderer().view().frameView().didPaintContents(context, paintDirtyRect, paintingState);

            InspectorInstrumentation::didPaint(layer.renderer(), paintDirtyRect);
        }

        ASSERT(!m_owningLayer.m_usedTransparency);
    };

    paintOneLayer(m_owningLayer, paintFlags);
    
    // FIXME: Need to check m_foregroundLayer, masking etc. webkit.org/b/197565.
    GraphicsLayer* destinationForSharingLayers = m_scrolledContentsLayer ? m_scrolledContentsLayer.get() : m_graphicsLayer.get();

    if (graphicsLayer == destinationForSharingLayers) {
        OptionSet<RenderLayer::PaintLayerFlag> sharingLayerPaintFlags = {
            RenderLayer::PaintLayerFlag::PaintingCompositingBackgroundPhase,
            RenderLayer::PaintLayerFlag::PaintingCompositingForegroundPhase };

        if (graphicsLayer->paintingPhase().contains(GraphicsLayerPaintingPhase::OverflowContents))
            sharingLayerPaintFlags.add(RenderLayer::PaintLayerFlag::PaintingOverflowContents);
        if (eventRegionContext)
            sharingLayerPaintFlags.add(RenderLayer::PaintLayerFlag::CollectingEventRegion);

        for (auto& layerWeakPtr : m_backingSharingLayers)
            paintOneLayer(*layerWeakPtr, sharingLayerPaintFlags);
    }

    if (!eventRegionContext)
        compositor().didPaintBacking(this);

#if USE(OWNING_LAYER_BEAR_TRAP)
    RELEASE_ASSERT_WITH_MESSAGE(m_owningLayerBearTrap == BEAR_TRAP_VALUE, "RenderLayerBacking::paintIntoLayer() end: m_owningLayerBearTrap caught the bear (55699292)");
    RELEASE_ASSERT_WITH_MESSAGE(&m_owningLayer, "RenderLayerBacking::paintIntoLayer() end: m_owningLayer is null (55699292)");
#endif
}

OptionSet<RenderLayer::PaintLayerFlag> RenderLayerBacking::paintFlagsForLayer(const GraphicsLayer& graphicsLayer) const
{
    OptionSet<RenderLayer::PaintLayerFlag> paintFlags;

    auto paintingPhase = graphicsLayer.paintingPhase();
    if (paintingPhase.contains(GraphicsLayerPaintingPhase::Background))
        paintFlags.add(RenderLayer::PaintLayerFlag::PaintingCompositingBackgroundPhase);
    if (paintingPhase.contains(GraphicsLayerPaintingPhase::Foreground))
        paintFlags.add(RenderLayer::PaintLayerFlag::PaintingCompositingForegroundPhase);
    if (paintingPhase.contains(GraphicsLayerPaintingPhase::Mask))
        paintFlags.add(RenderLayer::PaintLayerFlag::PaintingCompositingMaskPhase);
    if (paintingPhase.contains(GraphicsLayerPaintingPhase::ClipPath))
        paintFlags.add(RenderLayer::PaintLayerFlag::PaintingCompositingClipPathPhase);
    if (paintingPhase.contains(GraphicsLayerPaintingPhase::ChildClippingMask))
        paintFlags.add(RenderLayer::PaintLayerFlag::PaintingChildClippingMaskPhase);
    if (paintingPhase.contains(GraphicsLayerPaintingPhase::OverflowContents))
        paintFlags.add(RenderLayer::PaintLayerFlag::PaintingOverflowContents);
    if (paintingPhase.contains(GraphicsLayerPaintingPhase::CompositedScroll))
        paintFlags.add(RenderLayer::PaintLayerFlag::PaintingCompositingScrollingPhase);

    if (&graphicsLayer == m_backgroundLayer.get() && m_backgroundLayerPaintsFixedRootBackground)
        paintFlags.add({ RenderLayer::PaintLayerFlag::PaintingRootBackgroundOnly, RenderLayer::PaintLayerFlag::PaintingCompositingForegroundPhase }); // Need PaintLayerFlag::PaintingCompositingForegroundPhase to walk child layers.
    else if (compositor().fixedRootBackgroundLayer())
        paintFlags.add(RenderLayer::PaintLayerFlag::PaintingSkipRootBackground);

    return paintFlags;
}

#if ENABLE(TOUCH_ACTION_REGIONS) || ENABLE(WHEEL_EVENT_REGIONS)
struct PatternDescription {
    ASCIILiteral name;
    FloatSize phase;
    SRGBA<uint8_t> fillColor;
};

static RefPtr<Pattern> patternForDescription(PatternDescription description, FloatSize contentOffset, GraphicsContext& destContext)
{
    const FloatSize tileSize { 32, 18 };

    auto imageBuffer = destContext.createAlignedImageBuffer(tileSize);
    if (!imageBuffer)
        return nullptr;

    {
        GraphicsContext& imageContext = imageBuffer->context();

        FontCascadeDescription fontDescription;
        fontDescription.setOneFamily("Helvetica"_s);
        fontDescription.setSpecifiedSize(10);
        fontDescription.setComputedSize(10);
        fontDescription.setWeight(FontSelectionValue(500));
        FontCascade font(WTFMove(fontDescription), 0, 0);
        font.update(nullptr);

        TextRun textRun = TextRun(StringView { description.name });
        imageContext.setFillColor(description.fillColor);

        constexpr float textGap = 4;
        constexpr float yOffset = 12;
        imageContext.drawText(font, textRun, { textGap, yOffset }, 0);
    }

    AffineTransform patternOffsetTransform;
    patternOffsetTransform.translate(contentOffset + description.phase);
    patternOffsetTransform.scale(1 / destContext.scaleFactor());

    return Pattern::create({ imageBuffer.releaseNonNull() }, { true, true, patternOffsetTransform });
};
#endif

#if ENABLE(TOUCH_ACTION_REGIONS)
static RefPtr<Pattern> patternForTouchAction(TouchAction touchAction, FloatSize contentOffset, GraphicsContext& destContext)
{
    auto toIndex = [](TouchAction touchAction) -> unsigned {
        switch (touchAction) {
        case TouchAction::None:
            return 1;
        case TouchAction::Manipulation:
            return 2;
        case TouchAction::PanX:
            return 3;
        case TouchAction::PanY:
            return 4;
        case TouchAction::PinchZoom:
            return 5;
        case TouchAction::Auto:
            break;
        }
        return 0;
    };

    constexpr auto fillColor = Color::black.colorWithAlphaByte(128);

    static const PatternDescription patternDescriptions[] = {
        { "auto"_s, { }, fillColor },
        { "none"_s, { }, fillColor },
        { "manip"_s, { }, fillColor },
        { "pan-x"_s, { }, fillColor },
        { "pan-y"_s, { 0, 9 }, fillColor },
        { "p-z"_s, { 16, 4.5 }, fillColor },
    };
    
    auto actionIndex = toIndex(touchAction);
    if (!actionIndex || actionIndex >= ARRAY_SIZE(patternDescriptions))
        return nullptr;

    return patternForDescription(patternDescriptions[actionIndex], contentOffset, destContext);
}
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
static RefPtr<Pattern> patternForEventListenerRegionType(EventListenerRegionType type, FloatSize contentOffset, GraphicsContext& destContext)
{
    auto patternAndPhase = [&]() -> PatternDescription {
        switch (type) {
        case EventListenerRegionType::Wheel:
            return { "wheel"_s, { }, Color::darkGreen.colorWithAlphaByte(128) };
        case EventListenerRegionType::NonPassiveWheel:
            return { "sync"_s, { 0, 9 }, SRGBA<uint8_t> { 200, 0, 0, 128 } };
        case EventListenerRegionType::MouseClick:
            break;
        }
        ASSERT_NOT_REACHED();
        return { ""_s, { }, Color::black };
    }();

    return patternForDescription(patternAndPhase, contentOffset, destContext);
}
#endif

void RenderLayerBacking::paintDebugOverlays(const GraphicsLayer* graphicsLayer, GraphicsContext& context)
{
    auto& eventRegion = graphicsLayer->eventRegion();
    if (eventRegion.isEmpty())
        return;

    GraphicsContextStateSaver stateSaver(context);

    // The region is offset by offsetFromRenderer() so undo that.
    auto contentOffset = roundedIntSize(graphicsLayer->offsetFromRenderer());
    context.translate(contentOffset);

    auto visibleDebugOverlayRegions = OptionSet<DebugOverlayRegions>::fromRaw(renderer().settings().visibleDebugOverlayRegions());
    if (visibleDebugOverlayRegions.containsAny({ DebugOverlayRegions::TouchActionRegion, DebugOverlayRegions::WheelEventHandlerRegion })) {
        constexpr auto regionColor = Color::blue.colorWithAlphaByte(50);
        context.setFillColor(regionColor);
        for (auto rect : eventRegion.region().rects())
            context.fillRect(rect);
    }

#if ENABLE(TOUCH_ACTION_REGIONS)
    if (visibleDebugOverlayRegions.contains(DebugOverlayRegions::TouchActionRegion)) {
        const TouchAction touchActionList[] = {
            TouchAction::None,
            TouchAction::Manipulation,
            TouchAction::PanX,
            TouchAction::PanY,
            TouchAction::PinchZoom,
        };

        for (auto action : touchActionList) {
            auto* actionRegion = graphicsLayer->eventRegion().regionForTouchAction(action);
            if (!actionRegion)
                continue;

            auto fillPattern = patternForTouchAction(action, contentOffsetInCompositingLayer(), context);
            if (!fillPattern)
                continue;

            context.setFillPattern(fillPattern.releaseNonNull());
            for (auto rect : actionRegion->rects())
                context.fillRect(rect);
        }
    }
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    if (visibleDebugOverlayRegions.contains(DebugOverlayRegions::WheelEventHandlerRegion)) {
        for (auto type : { EventListenerRegionType::Wheel, EventListenerRegionType::NonPassiveWheel }) {
            auto fillPattern = patternForEventListenerRegionType(type, graphicsLayer->offsetFromRenderer(), context);
            context.setFillPattern(fillPattern.releaseNonNull());

            auto& region = graphicsLayer->eventRegion().eventListenerRegionForType(type);
            for (auto rect : region.rects())
                context.fillRect(rect);
        }
    }
#endif

#if ENABLE(EDITABLE_REGION)
    if (visibleDebugOverlayRegions.contains(DebugOverlayRegions::EditableElementRegion)) {
        context.setFillColor(SRGBA<uint8_t> { 128, 0, 128, 50 });
        for (auto rect : eventRegion.rectsForEditableElements())
            context.fillRect(rect);
    }
#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    if (DebugPageOverlays::shouldPaintOverlayIntoLayerForRegionType(renderer().page(), DebugPageOverlays::RegionType::InteractionRegion)) {
        context.setStrokeColor(Color::green);
        context.setStrokeThickness(1);

        for (const auto& region : eventRegion.interactionRegions()) {
            for (auto rect : region.regionInLayerCoordinates.rects()) {
                Path path;
                path.addRoundedRect(rect, { region.borderRadius, region.borderRadius });
                context.strokePath(path);
            }
        }
    }
#endif
}

// Up-call from compositing layer drawing callback.
void RenderLayerBacking::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, const FloatRect& clip, GraphicsLayerPaintBehavior layerPaintBehavior)
{
#ifndef NDEBUG
    renderer().page().setIsPainting(true);
#endif

#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(renderer().useDarkAppearance());
#endif

    // The dirtyRect is in the coords of the painting root.
    FloatRect adjustedClipRect = clip;
    adjustedClipRect.move(m_subpixelOffsetFromRenderer);
    IntRect dirtyRect = enclosingIntRect(adjustedClipRect);

    if (!graphicsLayer->repaintCount())
        layerPaintBehavior |= GraphicsLayerPaintFirstTilePaint;

    if (graphicsLayer == m_graphicsLayer.get()
        || graphicsLayer == m_foregroundLayer.get()
        || graphicsLayer == m_backgroundLayer.get()
        || graphicsLayer == m_maskLayer.get()
        || graphicsLayer == m_scrolledContentsLayer.get()) {

        if (!graphicsLayer->paintingPhase().contains(GraphicsLayerPaintingPhase::OverflowContents))
            dirtyRect.intersect(enclosingIntRect(compositedBoundsIncludingMargin()));

        // We have to use the same root as for hit testing, because both methods can compute and cache clipRects.
        OptionSet<PaintBehavior> behavior = PaintBehavior::Normal;
        if (layerPaintBehavior == GraphicsLayerPaintSnapshotting)
            behavior.add(PaintBehavior::Snapshotting);
        
        if (layerPaintBehavior == GraphicsLayerPaintFirstTilePaint)
            behavior.add(PaintBehavior::TileFirstPaint);

        paintIntoLayer(graphicsLayer, context, dirtyRect, behavior);

        auto visibleDebugOverlayRegions = OptionSet<DebugOverlayRegions>::fromRaw(renderer().settings().visibleDebugOverlayRegions());
        if (visibleDebugOverlayRegions.containsAny({ DebugOverlayRegions::TouchActionRegion, DebugOverlayRegions::EditableElementRegion, DebugOverlayRegions::WheelEventHandlerRegion, DebugOverlayRegions::InteractionRegion }))
            paintDebugOverlays(graphicsLayer, context);

    } else if (graphicsLayer == layerForHorizontalScrollbar()) {
        if (m_owningLayer.hasVisibleContent()) {
            auto* scrollableArea = m_owningLayer.scrollableArea();
            ASSERT(scrollableArea);

            paintScrollbar(scrollableArea->horizontalScrollbar(), context, dirtyRect);
        }
    } else if (graphicsLayer == layerForVerticalScrollbar()) {
        if (m_owningLayer.hasVisibleContent()) {
            auto* scrollableArea = m_owningLayer.scrollableArea();
            ASSERT(scrollableArea);

            paintScrollbar(scrollableArea->verticalScrollbar(), context, dirtyRect);
        }
    } else if (graphicsLayer == layerForScrollCorner()) {
        auto* scrollableArea = m_owningLayer.scrollableArea();
        ASSERT(scrollableArea);

        auto cornerRect = scrollableArea->overflowControlsRects().scrollCornerOrResizerRect();
        GraphicsContextStateSaver stateSaver(context);
        context.translate(-cornerRect.location());
        LayoutRect transformedClip = LayoutRect(clip);
        transformedClip.moveBy(cornerRect.location());
        scrollableArea->paintScrollCorner(context, IntPoint(), snappedIntRect(transformedClip));
        scrollableArea->paintResizer(context, IntPoint(), transformedClip);
    }
#ifndef NDEBUG
    renderer().page().setIsPainting(false);
#endif
}

float RenderLayerBacking::pageScaleFactor() const
{
    return compositor().pageScaleFactor();
}

float RenderLayerBacking::zoomedOutPageScaleFactor() const
{
    return compositor().zoomedOutPageScaleFactor();
}

float RenderLayerBacking::deviceScaleFactor() const
{
    return compositor().deviceScaleFactor();
}

float RenderLayerBacking::contentsScaleMultiplierForNewTiles(const GraphicsLayer* layer) const
{
    return compositor().contentsScaleMultiplierForNewTiles(layer);
}

bool RenderLayerBacking::paintsOpaquelyAtNonIntegralScales(const GraphicsLayer*) const
{
    return m_isMainFrameRenderViewLayer;
}

void RenderLayerBacking::didChangePlatformLayerForLayer(const GraphicsLayer* layer)
{
    compositor().didChangePlatformLayerForLayer(m_owningLayer, layer);
}

bool RenderLayerBacking::getCurrentTransform(const GraphicsLayer* graphicsLayer, TransformationMatrix& transform) const
{
    auto* transformedLayer = m_contentsContainmentLayer.get() ? m_contentsContainmentLayer.get() : m_graphicsLayer.get();
    if (graphicsLayer != transformedLayer)
        return false;

    if (m_owningLayer.hasTransform()) {
        transform = m_owningLayer.currentTransform(RenderStyle::individualTransformOperations);
        return true;
    }
    return false;
}

bool RenderLayerBacking::isTrackingRepaints() const
{
    return static_cast<GraphicsLayerClient&>(compositor()).isTrackingRepaints();
}

bool RenderLayerBacking::shouldSkipLayerInDump(const GraphicsLayer* layer, OptionSet<LayerTreeAsTextOptions> options) const
{
    if (options & LayerTreeAsTextOptions::Debug)
        return false;

    // Skip the root tile cache's flattening layer.
    return m_isMainFrameRenderViewLayer && layer && layer == m_childContainmentLayer.get();
}

bool RenderLayerBacking::shouldDumpPropertyForLayer(const GraphicsLayer* layer, const char* propertyName, OptionSet<LayerTreeAsTextOptions> options) const
{
    // For backwards compatibility with WebKit1 and other platforms,
    // skip some properties on the root tile cache.
    if (m_isMainFrameRenderViewLayer && layer == m_graphicsLayer.get() && !(options & LayerTreeAsTextOptions::IncludeRootLayerProperties)) {
        if (!strcmp(propertyName, "drawsContent"))
            return false;

        // Background color could be of interest to tests or other dumpers if it's non-white.
        if (!strcmp(propertyName, "backgroundColor") && Color::isWhiteColor(layer->backgroundColor()))
            return false;

        // The root tile cache's repaints will show up at the top with FrameView's,
        // so don't dump them twice.
        if (!strcmp(propertyName, "repaintRects"))
            return false;
    }

    return true;
}

bool RenderLayerBacking::shouldAggressivelyRetainTiles(const GraphicsLayer*) const
{
    // Only the main frame TileController has enough information about in-window state to
    // correctly implement aggressive tile retention.
    if (!m_isMainFrameRenderViewLayer)
        return false;

    return renderer().settings().aggressiveTileRetentionEnabled();
}

bool RenderLayerBacking::shouldTemporarilyRetainTileCohorts(const GraphicsLayer*) const
{
    return renderer().settings().temporaryTileCohortRetentionEnabled();
}

bool RenderLayerBacking::useGiantTiles() const
{
    return renderer().settings().useGiantTiles();
}

bool RenderLayerBacking::useCSS3DTransformInteroperability() const
{
    return renderer().settings().css3DTransformInteroperabilityEnabled();
}

void RenderLayerBacking::logFilledVisibleFreshTile(unsigned blankPixelCount)
{
    if (auto* loggingClient = renderer().page().performanceLoggingClient())
        loggingClient->logScrollingEvent(PerformanceLoggingClient::ScrollingEvent::FilledTile, MonotonicTime::now(), blankPixelCount);
}

#ifndef NDEBUG
void RenderLayerBacking::verifyNotPainting()
{
    ASSERT(!renderer().page().isPainting());
}
#endif

bool RenderLayerBacking::startAnimation(double timeOffset, const Animation& animation, const KeyframeList& keyframes)
{
    bool hasOpacity = keyframes.containsProperty(CSSPropertyOpacity);
    bool hasRotate = renderer().isBox() && keyframes.containsProperty(CSSPropertyRotate);
    bool hasScale = renderer().isBox() && keyframes.containsProperty(CSSPropertyScale);
    bool hasTranslate = renderer().isBox() && keyframes.containsProperty(CSSPropertyTranslate);
    bool hasTransform = renderer().isBox() && keyframes.containsProperty(CSSPropertyTransform);
    bool hasFilter = keyframes.containsProperty(CSSPropertyFilter);

    bool hasBackdropFilter = false;
#if ENABLE(FILTERS_LEVEL_2)
    hasBackdropFilter = keyframes.containsProperty(CSSPropertyWebkitBackdropFilter);
#endif

    if (!hasOpacity && !hasRotate && !hasScale && !hasTranslate && !hasTransform && !hasFilter && !hasBackdropFilter)
        return false;

    KeyframeValueList rotateVector(AnimatedPropertyRotate);
    KeyframeValueList scaleVector(AnimatedPropertyScale);
    KeyframeValueList translateVector(AnimatedPropertyTranslate);
    KeyframeValueList transformVector(AnimatedPropertyTransform);
    KeyframeValueList opacityVector(AnimatedPropertyOpacity);
    KeyframeValueList filterVector(AnimatedPropertyFilter);
#if ENABLE(FILTERS_LEVEL_2)
    KeyframeValueList backdropFilterVector(AnimatedPropertyWebkitBackdropFilter);
#endif

    for (auto& currentKeyframe : keyframes) {
        const RenderStyle* keyframeStyle = currentKeyframe.style();
        double key = currentKeyframe.key();

        if (!keyframeStyle)
            continue;
            
        auto* tf = currentKeyframe.timingFunction();
        
        if (currentKeyframe.containsProperty(CSSPropertyRotate))
            rotateVector.insert(makeUnique<TransformAnimationValue>(key, keyframeStyle->rotate(), tf));

        if (currentKeyframe.containsProperty(CSSPropertyScale))
            scaleVector.insert(makeUnique<TransformAnimationValue>(key, keyframeStyle->scale(), tf));

        if (currentKeyframe.containsProperty(CSSPropertyTranslate))
            translateVector.insert(makeUnique<TransformAnimationValue>(key, keyframeStyle->translate(), tf));

        if (currentKeyframe.containsProperty(CSSPropertyTransform))
            transformVector.insert(makeUnique<TransformAnimationValue>(key, keyframeStyle->transform(), tf));

        if (currentKeyframe.containsProperty(CSSPropertyOpacity))
            opacityVector.insert(makeUnique<FloatAnimationValue>(key, keyframeStyle->opacity(), tf));

        if (currentKeyframe.containsProperty(CSSPropertyFilter))
            filterVector.insert(makeUnique<FilterAnimationValue>(key, keyframeStyle->filter(), tf));

#if ENABLE(FILTERS_LEVEL_2)
        if (currentKeyframe.containsProperty(CSSPropertyWebkitBackdropFilter))
            backdropFilterVector.insert(makeUnique<FilterAnimationValue>(key, keyframeStyle->backdropFilter(), tf));
#endif
    }

    if (!renderer().settings().acceleratedCompositedAnimationsEnabled())
        return false;

    bool didAnimate = false;

    if (hasRotate && m_graphicsLayer->addAnimation(rotateVector, snappedIntRect(m_owningLayer.rendererBorderBoxRect()).size(), &animation, keyframes.animationName(), timeOffset))
        didAnimate = true;

    if (hasScale && m_graphicsLayer->addAnimation(scaleVector, snappedIntRect(m_owningLayer.rendererBorderBoxRect()).size(), &animation, keyframes.animationName(), timeOffset))
        didAnimate = true;

    if (hasTranslate && m_graphicsLayer->addAnimation(translateVector, snappedIntRect(m_owningLayer.rendererBorderBoxRect()).size(), &animation, keyframes.animationName(), timeOffset))
        didAnimate = true;

    if (hasTransform && m_graphicsLayer->addAnimation(transformVector, snappedIntRect(m_owningLayer.rendererBorderBoxRect()).size(), &animation, keyframes.animationName(), timeOffset))
        didAnimate = true;

    if (hasOpacity && m_graphicsLayer->addAnimation(opacityVector, IntSize { }, &animation, keyframes.animationName(), timeOffset))
        didAnimate = true;

    if (hasFilter && m_graphicsLayer->addAnimation(filterVector, IntSize { }, &animation, keyframes.animationName(), timeOffset))
        didAnimate = true;

#if ENABLE(FILTERS_LEVEL_2)
    if (hasBackdropFilter && m_graphicsLayer->addAnimation(backdropFilterVector, IntSize { }, &animation, keyframes.animationName(), timeOffset))
        didAnimate = true;
#endif

    if (didAnimate) {
        m_owningLayer.setNeedsPostLayoutCompositingUpdate();
        m_owningLayer.setNeedsCompositingGeometryUpdate();
    }

    return didAnimate;
}

void RenderLayerBacking::animationPaused(double timeOffset, const String& animationName)
{
    m_graphicsLayer->pauseAnimation(animationName, timeOffset);
}

void RenderLayerBacking::animationFinished(const String& animationName)
{
    m_graphicsLayer->removeAnimation(animationName);
    m_owningLayer.setNeedsPostLayoutCompositingUpdate();
    m_owningLayer.setNeedsCompositingGeometryUpdate();
}

void RenderLayerBacking::transformRelatedPropertyDidChange()
{
    m_graphicsLayer->transformRelatedPropertyDidChange();
}

void RenderLayerBacking::notifyAnimationStarted(const GraphicsLayer*, const String&, MonotonicTime)
{
}

void RenderLayerBacking::notifyFlushRequired(const GraphicsLayer* layer)
{
    if (renderer().renderTreeBeingDestroyed())
        return;

    compositor().notifyFlushRequired(layer);
}

void RenderLayerBacking::notifyFlushBeforeDisplayRefresh(const GraphicsLayer* layer)
{
    compositor().notifyFlushBeforeDisplayRefresh(layer);
}

// This is used for the 'freeze' API, for testing only.
void RenderLayerBacking::suspendAnimations(MonotonicTime time)
{
    m_graphicsLayer->suspendAnimations(time);
}

void RenderLayerBacking::resumeAnimations()
{
    m_graphicsLayer->resumeAnimations();
}

LayoutRect RenderLayerBacking::compositedBounds() const
{
    return m_compositedBounds;
}

bool RenderLayerBacking::setCompositedBounds(const LayoutRect& bounds)
{
    if (bounds == m_compositedBounds)
        return false;

    m_compositedBounds = bounds;
    return true;
}

LayoutRect RenderLayerBacking::compositedBoundsIncludingMargin() const
{
    auto* tiledBacking = this->tiledBacking();
    if (!tiledBacking || !tiledBacking->hasMargins())
        return compositedBounds();

    LayoutRect boundsIncludingMargin = compositedBounds();
    LayoutUnit leftMarginWidth = tiledBacking->leftMarginWidth();
    LayoutUnit topMarginHeight = tiledBacking->topMarginHeight();

    boundsIncludingMargin.moveBy(LayoutPoint(-leftMarginWidth, -topMarginHeight));
    boundsIncludingMargin.expand(leftMarginWidth + tiledBacking->rightMarginWidth(), topMarginHeight + tiledBacking->bottomMarginHeight());

    return boundsIncludingMargin;
}

CSSPropertyID RenderLayerBacking::graphicsLayerToCSSProperty(AnimatedPropertyID property)
{
    CSSPropertyID cssProperty = CSSPropertyInvalid;
    switch (property) {
    case AnimatedPropertyTranslate:
        cssProperty = CSSPropertyTranslate;
        break;
    case AnimatedPropertyScale:
        cssProperty = CSSPropertyScale;
        break;
    case AnimatedPropertyRotate:
        cssProperty = CSSPropertyRotate;
        break;
    case AnimatedPropertyTransform:
        cssProperty = CSSPropertyTransform;
        break;
    case AnimatedPropertyOpacity:
        cssProperty = CSSPropertyOpacity;
        break;
    case AnimatedPropertyBackgroundColor:
        cssProperty = CSSPropertyBackgroundColor;
        break;
    case AnimatedPropertyFilter:
        cssProperty = CSSPropertyFilter;
        break;
#if ENABLE(FILTERS_LEVEL_2)
    case AnimatedPropertyWebkitBackdropFilter:
        cssProperty = CSSPropertyWebkitBackdropFilter;
        break;
#endif
    case AnimatedPropertyInvalid:
        ASSERT_NOT_REACHED();
    }
    return cssProperty;
}

AnimatedPropertyID RenderLayerBacking::cssToGraphicsLayerProperty(CSSPropertyID cssProperty)
{
    switch (cssProperty) {
    case CSSPropertyTranslate:
        return AnimatedPropertyTranslate;
    case CSSPropertyScale:
        return AnimatedPropertyScale;
    case CSSPropertyRotate:
        return AnimatedPropertyRotate;
    case CSSPropertyTransform:
        return AnimatedPropertyTransform;
    case CSSPropertyOpacity:
        return AnimatedPropertyOpacity;
    case CSSPropertyBackgroundColor:
        return AnimatedPropertyBackgroundColor;
    case CSSPropertyFilter:
        return AnimatedPropertyFilter;
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
        return AnimatedPropertyWebkitBackdropFilter;
#endif
    default:
        // It's fine if we see other css properties here; they are just not accelerated.
        break;
    }
    return AnimatedPropertyInvalid;
}

CompositingLayerType RenderLayerBacking::compositingLayerType() const
{
    if (m_graphicsLayer->usesContentsLayer())
        return MediaCompositingLayer;

    if (m_graphicsLayer->drawsContent())
        return m_graphicsLayer->tiledBacking() ? TiledCompositingLayer : NormalCompositingLayer;
    
    return ContainerCompositingLayer;
}

double RenderLayerBacking::backingStoreMemoryEstimate() const
{
    double backingMemory;
    
    // Layers in m_ancestorClippingStack, m_contentsContainmentLayer and m_childContainmentLayer are just used for masking or containment, so have no backing.
    backingMemory = m_graphicsLayer->backingStoreMemoryEstimate();
    if (m_foregroundLayer)
        backingMemory += m_foregroundLayer->backingStoreMemoryEstimate();
    if (m_backgroundLayer)
        backingMemory += m_backgroundLayer->backingStoreMemoryEstimate();
    if (m_maskLayer)
        backingMemory += m_maskLayer->backingStoreMemoryEstimate();

    if (m_scrolledContentsLayer)
        backingMemory += m_scrolledContentsLayer->backingStoreMemoryEstimate();

    if (m_layerForHorizontalScrollbar)
        backingMemory += m_layerForHorizontalScrollbar->backingStoreMemoryEstimate();

    if (m_layerForVerticalScrollbar)
        backingMemory += m_layerForVerticalScrollbar->backingStoreMemoryEstimate();

    if (m_layerForScrollCorner)
        backingMemory += m_layerForScrollCorner->backingStoreMemoryEstimate();
    
    return backingMemory;
}

TextStream& operator<<(TextStream& ts, const RenderLayerBacking& backing)
{
    ts << "RenderLayerBacking " << &backing << " bounds " << backing.compositedBounds();

    if (backing.isFrameLayerWithTiledBacking())
        ts << " frame layer tiled backing";
    if (backing.paintsIntoWindow())
        ts << " paintsIntoWindow";
    if (backing.paintsIntoCompositedAncestor())
        ts << " paintsIntoCompositedAncestor";

    ts << " primary layer ID " << backing.graphicsLayer()->primaryLayerID();
    if (auto nodeID = backing.scrollingNodeIDForRole(ScrollCoordinationRole::ViewportConstrained))
        ts << " viewport constrained scrolling node " << nodeID;
    if (auto nodeID = backing.scrollingNodeIDForRole(ScrollCoordinationRole::Scrolling))
        ts << " scrolling node " << nodeID;

    if (backing.ancestorClippingStack())
        ts << " ancestor clip stack " << *backing.ancestorClippingStack();

    if (auto nodeID = backing.scrollingNodeIDForRole(ScrollCoordinationRole::FrameHosting))
        ts << " frame hosting node " << nodeID;
    if (auto nodeID = backing.scrollingNodeIDForRole(ScrollCoordinationRole::Positioning))
        ts << " positioning node " << nodeID;
    return ts;
}

TransformationMatrix RenderLayerBacking::transformMatrixForProperty(AnimatedPropertyID property) const
{
    TransformationMatrix matrix;

    auto applyTransformOperation = [&](TransformOperation* operation) {
        if (operation)
            operation->apply(matrix, snappedIntRect(m_owningLayer.rendererBorderBoxRect()).size());
    };

    if (property == AnimatedPropertyTranslate)
        applyTransformOperation(renderer().style().translate());
    else if (property == AnimatedPropertyScale)
        applyTransformOperation(renderer().style().scale());
    else if (property == AnimatedPropertyRotate)
        applyTransformOperation(renderer().style().rotate());
    else if (property == AnimatedPropertyTransform)
        renderer().style().transform().apply(snappedIntRect(m_owningLayer.rendererBorderBoxRect()).size(), matrix);
    else
        ASSERT_NOT_REACHED();

    return matrix;
}

} // namespace WebCore
