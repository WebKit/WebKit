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

#include "BitmapImage.h"
#include "CSSAnimationController.h"
#include "CanvasRenderingContext.h"
#include "CSSPropertyNames.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "DocumentTimeline.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "InspectorInstrumentation.h"
#include "KeyframeList.h"
#include "Logging.h"
#include "Page.h"
#include "PerformanceLoggingClient.h"
#include "PluginViewBase.h"
#include "ProgressTracker.h"
#include "RenderFragmentContainer.h"
#include "RenderFragmentedFlow.h"
#include "RenderHTMLCanvas.h"
#include "RenderIFrame.h"
#include "RenderImage.h"
#include "RenderLayerCompositor.h"
#include "RenderEmbeddedObject.h"
#include "RenderMedia.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "TiledBacking.h"
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

#if ENABLE(ACCELERATED_2D_CANVAS)
    return CanvasAsLayerContents;
#else
    return CanvasPaintedToLayer; // On Mac and iOS we paint accelerated canvases into their layers.
#endif
}

// This acts as a cache of what we know about what is painting into this RenderLayerBacking.
class PaintedContentsInfo {
public:
    enum class ContentsTypeDetermination {
        Unknown,
        SimpleContainer,
        DirectlyCompositedImage,
        Painted
    };

    PaintedContentsInfo(RenderLayerBacking& inBacking)
        : m_backing(inBacking)
    {
    }
    
    void setWantsSubpixelAntialiasedTextState(bool wantsSubpixelAntialiasedTextState)
    {
        m_subpixelAntialiasedText = wantsSubpixelAntialiasedTextState ? RequestState::Unknown : RequestState::DontCare;
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

    RequestState paintsSubpixelAntialiasedTextDetermination();
    bool paintsSubpixelAntialiasedText()
    {
        RequestState state = paintsSubpixelAntialiasedTextDetermination();
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

    RenderLayerBacking& m_backing;
    RequestState m_boxDecorations { RequestState::Unknown };
    RequestState m_content { RequestState::Unknown };
    RequestState m_subpixelAntialiasedText { RequestState::DontCare };

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
    if (m_content != RequestState::Unknown && m_subpixelAntialiasedText != RequestState::Unknown)
        return m_content;

    RenderLayer::PaintedContentRequest contentRequest;
    if (m_subpixelAntialiasedText == RequestState::Unknown)
        contentRequest.hasSubpixelAntialiasedText = RequestState::Unknown;

    m_content = m_backing.paintsContent(contentRequest) ? RequestState::True : RequestState::False;

    if (m_subpixelAntialiasedText == RequestState::Unknown)
        m_subpixelAntialiasedText = contentRequest.hasSubpixelAntialiasedText;

    return m_content;
}

RequestState PaintedContentsInfo::paintsSubpixelAntialiasedTextDetermination()
{
    if (m_subpixelAntialiasedText != RequestState::Unknown)
        return m_subpixelAntialiasedText;

    paintsContentDetermination();

    return m_subpixelAntialiasedText;
}

PaintedContentsInfo::ContentsTypeDetermination PaintedContentsInfo::contentsTypeDetermination()
{
    if (m_contentsType != ContentsTypeDetermination::Unknown)
        return m_contentsType;

    if (m_backing.isSimpleContainerCompositingLayer(*this))
        m_contentsType = ContentsTypeDetermination::SimpleContainer;
    else if (m_backing.isDirectlyCompositedImage())
        m_contentsType = ContentsTypeDetermination::DirectlyCompositedImage;
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
            tiledBacking->setScrollingPerformanceLoggingEnabled(renderer().settings().scrollingPerformanceLoggingEnabled());
            adjustTiledBackingCoverage();
        }
    }
}

RenderLayerBacking::~RenderLayerBacking()
{
    updateAncestorClippingLayer(false);
    updateChildClippingStrategy(false);
    updateDescendantClippingLayer(false);
    updateOverflowControlsLayers(false, false, false);
    updateForegroundLayer(false);
    updateBackgroundLayer(false);
    updateMaskingLayer(false, false);
    updateScrollingLayers(false);
    detachFromScrollingCoordinator({ ScrollCoordinationRole::Scrolling, ScrollCoordinationRole::ViewportConstrained, ScrollCoordinationRole::FrameHosting });
    destroyGraphicsLayers();
}

void RenderLayerBacking::willDestroyLayer(const GraphicsLayer* layer)
{
    if (layer && layer->type() == GraphicsLayer::Type::Normal && layer->tiledBacking())
        compositor().layerTiledBackingUsageChanged(layer, false);
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

String RenderLayerBacking::displayListAsText(DisplayList::AsTextFlags flags) const
{
    return m_graphicsLayer->displayListAsText(flags);
}

void RenderLayerBacking::setIsTrackingDisplayListReplay(bool isTrackingReplay)
{
    m_graphicsLayer->setIsTrackingDisplayListReplay(isTrackingReplay);
}

String RenderLayerBacking::replayDisplayListAsText(DisplayList::AsTextFlags flags) const
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

static TiledBacking::TileCoverage computePageTiledBackingCoverage(RenderLayerBacking* backing)
{
    // FIXME: When we use TiledBacking for overflow, this should look at RenderView scrollability.
    auto& frameView = backing->owningLayer().renderer().view().frameView();

    // If the page is non-visible, don't incur the cost of keeping extra tiles for scrolling.
    if (!backing->owningLayer().page().isVisible())
        return TiledBacking::CoverageForVisibleArea;

    TiledBacking::TileCoverage tileCoverage = TiledBacking::CoverageForVisibleArea;
    bool useMinimalTilesDuringLiveResize = frameView.inLiveResize();
    if (frameView.speculativeTilingEnabled() && !useMinimalTilesDuringLiveResize) {
        bool clipsToExposedRect = static_cast<bool>(frameView.viewExposedRect());
        if (frameView.horizontalScrollbarMode() != ScrollbarAlwaysOff || clipsToExposedRect)
            tileCoverage |= TiledBacking::CoverageForHorizontalScrolling;

        if (frameView.verticalScrollbarMode() != ScrollbarAlwaysOff || clipsToExposedRect)
            tileCoverage |= TiledBacking::CoverageForVerticalScrolling;
    }
    return tileCoverage;
}

void RenderLayerBacking::adjustTiledBackingCoverage()
{
    if (!m_isFrameLayerWithTiledBacking)
        return;

    TiledBacking::TileCoverage tileCoverage = computePageTiledBackingCoverage(this);
    tiledBacking()->setTileCoverage(tileCoverage);
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
    
    if (m_ancestorClippingLayer)
        m_ancestorClippingLayer->setShowDebugBorder(showBorder);

    if (m_foregroundLayer) {
        m_foregroundLayer->setShowDebugBorder(showBorder);
        m_foregroundLayer->setShowRepaintCounter(showRepaintCounter);
    }
    
    if (m_contentsContainmentLayer)
        m_contentsContainmentLayer->setShowDebugBorder(showBorder);
    
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

    if (m_scrollingLayer)
        m_scrollingLayer->setShowDebugBorder(showBorder);

    if (m_scrollingContentsLayer) {
        m_scrollingContentsLayer->setShowDebugBorder(showBorder);
        m_scrollingContentsLayer->setShowRepaintCounter(showRepaintCounter);
    }
}

void RenderLayerBacking::createPrimaryGraphicsLayer()
{
    String layerName = m_owningLayer.name();
    const unsigned maxLayerNameLength = 100;
    if (layerName.length() > maxLayerNameLength) {
        layerName.truncate(maxLayerNameLength);
        layerName.append("...");
    }
    m_graphicsLayer = createGraphicsLayer(layerName, m_isFrameLayerWithTiledBacking ? GraphicsLayer::Type::PageTiledBacking : GraphicsLayer::Type::Normal);

    if (m_isFrameLayerWithTiledBacking) {
        m_childContainmentLayer = createGraphicsLayer("Page TiledBacking containment");
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
    
    updateOpacity(renderer().style());
    updateTransform(renderer().style());
    updateFilters(renderer().style());
#if ENABLE(FILTERS_LEVEL_2)
    updateBackdropFilters(renderer().style());
#endif
#if ENABLE(CSS_COMPOSITING)
    updateBlendMode(renderer().style());
#endif
    updateCustomAppearance(renderer().style());
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

    GraphicsLayer::unparentAndClear(m_ancestorClippingLayer);
    GraphicsLayer::unparentAndClear(m_contentsContainmentLayer);
    GraphicsLayer::unparentAndClear(m_foregroundLayer);
    GraphicsLayer::unparentAndClear(m_backgroundLayer);
    GraphicsLayer::unparentAndClear(m_childContainmentLayer);
    GraphicsLayer::unparentAndClear(m_childClippingMaskLayer);
    GraphicsLayer::unparentAndClear(m_scrollingLayer);
    GraphicsLayer::unparentAndClear(m_scrollingContentsLayer);
    GraphicsLayer::unparentAndClear(m_graphicsLayer);
}

void RenderLayerBacking::updateOpacity(const RenderStyle& style)
{
    m_graphicsLayer->setOpacity(compositingOpacity(style.opacity()));
}

void RenderLayerBacking::updateTransform(const RenderStyle& style)
{
    // FIXME: This could use m_owningLayer.transform(), but that currently has transform-origin
    // baked into it, and we don't want that.
    TransformationMatrix t;
    if (m_owningLayer.hasTransform()) {
        auto& renderBox = downcast<RenderBox>(renderer());
        style.applyTransform(t, snapRectToDevicePixels(renderBox.borderBoxRect(), deviceScaleFactor()), RenderStyle::ExcludeTransformOrigin);
        makeMatrixRenderable(t, compositor().canRender3DTransforms());
    }
    
    if (m_contentsContainmentLayer) {
        m_contentsContainmentLayer->setTransform(t);
        m_graphicsLayer->setTransform(TransformationMatrix());
    } else
        m_graphicsLayer->setTransform(t);
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

    auto& renderer = downcast<RenderBox>(this->renderer());
    LayoutRect boxRect = renderer.borderBoxRect();
    if (renderer.hasClip())
        boxRect.intersect(renderer.clipRect(LayoutPoint(), nullptr));
    boxRect.move(contentOffsetInCompostingLayer());

    FloatRoundedRect backdropFiltersRect;
    if (renderer.style().hasBorderRadius() && !renderer.hasClip())
        backdropFiltersRect = renderer.style().getRoundedInnerBorderFor(boxRect).pixelSnappedRoundedRectForPainting(deviceScaleFactor());
    else
        backdropFiltersRect = FloatRoundedRect(snapRectToDevicePixels(boxRect, deviceScaleFactor()));

    m_graphicsLayer->setBackdropFiltersRect(backdropFiltersRect);
}
#endif

#if ENABLE(CSS_COMPOSITING)
void RenderLayerBacking::updateBlendMode(const RenderStyle& style)
{
    // FIXME: where is the blend mode updated when m_ancestorClippingLayers come and go?
    if (m_ancestorClippingLayer) {
        m_ancestorClippingLayer->setBlendMode(style.blendMode());
        m_graphicsLayer->setBlendMode(BlendMode::Normal);
    } else
        m_graphicsLayer->setBlendMode(style.blendMode());
}
#endif

void RenderLayerBacking::updateCustomAppearance(const RenderStyle& style)
{
    ControlPart appearance = style.appearance();
    if (appearance == MediaControlsLightBarBackgroundPart)
        m_graphicsLayer->setCustomAppearance(GraphicsLayer::CustomAppearance::LightBackdrop);
    else if (appearance == MediaControlsDarkBarBackgroundPart)
        m_graphicsLayer->setCustomAppearance(GraphicsLayer::CustomAppearance::DarkBackdrop);
    else
        m_graphicsLayer->setCustomAppearance(GraphicsLayer::CustomAppearance::None);
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
    return (style.transformOriginX().type() == Fixed && style.transformOriginX().value())
        || (style.transformOriginY().type() == Fixed && style.transformOriginY().value());
}

bool RenderLayerBacking::updateCompositedBounds()
{
    LayoutRect layerBounds = m_owningLayer.calculateLayerBounds(&m_owningLayer, LayoutSize(), RenderLayer::defaultCalculateLayerBoundsFlags() | RenderLayer::ExcludeHiddenDescendants | RenderLayer::DontConstrainForMask);
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

void RenderLayerBacking::updateAfterWidgetResize()
{
    if (!is<RenderWidget>(renderer()))
        return;

    if (auto* innerCompositor = RenderLayerCompositor::frameContentsCompositor(downcast<RenderWidget>(renderer()))) {
        innerCompositor->frameViewDidChangeSize();
        innerCompositor->frameViewDidChangeLocation(flooredIntPoint(contentsBox().location()));
    }
}

void RenderLayerBacking::updateAfterLayout(bool needsFullRepaint)
{
    LOG(Compositing, "RenderLayerBacking %p updateAfterLayout (layer %p)", this, &m_owningLayer);

    // This is the main trigger for layout changing layer geometry, but we have to do the work again in updateBackingAndHierarchy()
    // when we know the final compositing hierarchy. We can't just set dirty bits from RenderLayer::setSize() because that doesn't
    // take overflow into account.
    if (updateCompositedBounds()) {
        m_owningLayer.setNeedsCompositingGeometryUpdate();
        // This layer's geometry affects those of its children.
        m_owningLayer.setChildrenNeedCompositingGeometryUpdate();
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
    updateCustomAppearance(style);
}

bool RenderLayerBacking::updateConfiguration()
{
    ASSERT(!m_owningLayer.normalFlowListDirty());
    ASSERT(!m_owningLayer.zOrderListsDirty());
    ASSERT(!renderer().view().needsLayout());

    bool layerConfigChanged = false;

    setBackgroundLayerPaintsFixedRootBackground(compositor().needsFixedRootBackgroundLayer(m_owningLayer));

    if (updateBackgroundLayer(m_backgroundLayerPaintsFixedRootBackground || m_requiresBackgroundLayer))
        layerConfigChanged = true;

    if (updateForegroundLayer(compositor().needsContentsCompositingLayer(m_owningLayer)))
        layerConfigChanged = true;
    
    // This requires descendants to have been updated.
    bool needsDescendantsClippingLayer = compositor().clipsCompositingDescendants(m_owningLayer);
    bool usesCompositedScrolling = m_owningLayer.hasCompositedScrollableOverflow();

    // Our scrolling layer will clip.
    if (usesCompositedScrolling)
        needsDescendantsClippingLayer = false;

    if (updateScrollingLayers(usesCompositedScrolling))
        layerConfigChanged = true;

    if (updateDescendantClippingLayer(needsDescendantsClippingLayer))
        layerConfigChanged = true;

    // clippedByAncestor() does a tree walk.
    if (updateAncestorClippingLayer(compositor().clippedByAncestor(m_owningLayer)))
        layerConfigChanged = true;

    if (updateOverflowControlsLayers(requiresHorizontalScrollbarLayer(), requiresVerticalScrollbarLayer(), requiresScrollCornerLayer()))
        layerConfigChanged = true;

    if (layerConfigChanged)
        updateInternalHierarchy();

    if (auto* flatteningLayer = tileCacheFlatteningLayer()) {
        if (layerConfigChanged || flatteningLayer->parent() != m_graphicsLayer.get())
            m_graphicsLayer->addChild(*flatteningLayer);
    }

    updateMaskingLayer(renderer().hasMask(), renderer().hasClipPath());

    updateChildClippingStrategy(needsDescendantsClippingLayer);

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

    if (is<RenderEmbeddedObject>(renderer()) && downcast<RenderEmbeddedObject>(renderer()).allowsAcceleratedCompositing()) {
        auto* pluginViewBase = downcast<PluginViewBase>(downcast<RenderWidget>(renderer()).widget());
#if PLATFORM(IOS_FAMILY)
        if (pluginViewBase && !m_graphicsLayer->contentsLayerForMedia()) {
            pluginViewBase->detachPluginLayer();
            pluginViewBase->attachPluginLayer();
        }
#else
        if (!pluginViewBase->shouldNotAddLayer())
            m_graphicsLayer->setContentsToPlatformLayer(pluginViewBase->platformLayer(), GraphicsLayer::ContentsLayerPurpose::Plugin);
#endif
    }
#if ENABLE(VIDEO)
    else if (is<RenderVideo>(renderer()) && downcast<RenderVideo>(renderer()).shouldDisplayVideo()) {
        auto* mediaElement = downcast<HTMLMediaElement>(renderer().element());
        m_graphicsLayer->setContentsToPlatformLayer(mediaElement->platformLayer(), GraphicsLayer::ContentsLayerPurpose::Media);
        // Requires layout.
        resetContentsRect();
    }
#endif
#if ENABLE(WEBGL) || ENABLE(ACCELERATED_2D_CANVAS)
    else if (renderer().isCanvas() && canvasCompositingStrategy(renderer()) == CanvasAsLayerContents) {
        const HTMLCanvasElement* canvas = downcast<HTMLCanvasElement>(renderer().element());
        if (auto* context = canvas->renderingContext())
            m_graphicsLayer->setContentsToPlatformLayer(context->platformLayer(), GraphicsLayer::ContentsLayerPurpose::Canvas);

        layerConfigChanged = true;
    }
#endif
    if (is<RenderWidget>(renderer()) && compositor().parentFrameContentLayers(downcast<RenderWidget>(renderer()))) {
        m_owningLayer.setNeedsCompositingGeometryUpdate();
        layerConfigChanged = true;
    }

    if (is<RenderImage>(renderer()) && downcast<RenderImage>(renderer()).isEditableImage()) {
        auto element = renderer().element();
        if (is<HTMLImageElement>(element)) {
            m_graphicsLayer->setContentsToEmbeddedView(GraphicsLayer::ContentsLayerEmbeddedViewType::EditableImage, downcast<HTMLImageElement>(element)->editableImageViewID());
            layerConfigChanged = true;
        }
    }

    return layerConfigChanged;
}

static LayoutRect clipBox(RenderBox& renderer)
{
    LayoutRect result = LayoutRect::infiniteRect();
    if (renderer.hasOverflowClip())
        result = renderer.overflowClipRect(LayoutPoint(), 0); // FIXME: Incorrect for CSS regions.

    if (renderer.hasClip())
        result.intersect(renderer.clipRect(LayoutPoint(), 0)); // FIXME: Incorrect for CSS regions.

    return result;
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
    
static SnappedRectInfo snappedGraphicsLayer(const LayoutSize& offset, const LayoutSize& size, float deviceScaleFactor)
{
    SnappedRectInfo snappedGraphicsLayer;
    LayoutRect graphicsLayerRect = LayoutRect(toLayoutPoint(offset), size);
    snappedGraphicsLayer.m_snappedRect = LayoutRect(snapRectToDevicePixels(graphicsLayerRect, deviceScaleFactor));
    snappedGraphicsLayer.m_snapDelta = snappedGraphicsLayer.m_snappedRect.location() - toLayoutPoint(offset);
    return snappedGraphicsLayer;
}

static LayoutSize computeOffsetFromAncestorGraphicsLayer(RenderLayer* compositedAncestor, const LayoutPoint& location, float deviceScaleFactor)
{
    if (!compositedAncestor)
        return toLayoutSize(location);

    // FIXME: This is a workaround until after webkit.org/162634 gets fixed. ancestorSubpixelOffsetFromRenderer
    // could be stale when a dynamic composited state change triggers a pre-order updateGeometry() traversal.
    LayoutSize ancestorSubpixelOffsetFromRenderer = compositedAncestor->backing()->subpixelOffsetFromRenderer();
    LayoutRect ancestorCompositedBounds = compositedAncestor->backing()->compositedBounds();
    LayoutSize floored = toLayoutSize(LayoutPoint(floorPointToDevicePixels(ancestorCompositedBounds.location() - ancestorSubpixelOffsetFromRenderer, deviceScaleFactor)));
    LayoutSize ancestorRendererOffsetFromAncestorGraphicsLayer = -(floored + ancestorSubpixelOffsetFromRenderer);
    return ancestorRendererOffsetFromAncestorGraphicsLayer + toLayoutSize(location);
}

class ComputedOffsets {
public:
    ComputedOffsets(const RenderLayer& renderLayer, const LayoutRect& localRect, const LayoutRect& parentGraphicsLayerRect, const LayoutRect& primaryGraphicsLayerRect)
        : m_renderLayer(renderLayer)
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
            auto* compositedAncestor = m_renderLayer.ancestorCompositingLayer();
            LayoutPoint localPointInAncestorRenderLayerCoords = m_renderLayer.convertToLayerCoords(compositedAncestor, m_location, RenderLayer::AdjustForColumns);
            m_fromAncestorGraphicsLayer = computeOffsetFromAncestorGraphicsLayer(compositedAncestor, localPointInAncestorRenderLayerCoords, m_deviceScaleFactor);
        }
        return m_fromAncestorGraphicsLayer.value();
    }

    Optional<LayoutSize> m_fromAncestorGraphicsLayer;
    Optional<LayoutSize> m_fromParentGraphicsLayer;
    Optional<LayoutSize> m_fromPrimaryGraphicsLayer;
    
    const RenderLayer& m_renderLayer;
    // Location is relative to the renderer.
    const LayoutPoint m_location;
    const LayoutSize m_parentGraphicsLayerOffset;
    const LayoutSize m_primaryGraphicsLayerOffset;
    float m_deviceScaleFactor;
};

LayoutRect RenderLayerBacking::computePrimaryGraphicsLayerRect(const LayoutRect& parentGraphicsLayerRect) const
{
    ComputedOffsets compositedBoundsOffset(m_owningLayer, compositedBounds(), parentGraphicsLayerRect, LayoutRect());
    return LayoutRect(encloseRectToDevicePixels(LayoutRect(toLayoutPoint(compositedBoundsOffset.fromParentGraphicsLayer()), compositedBounds().size()),
        deviceScaleFactor()));
}

// FIXME: See if we need this now that updateGeometry() is always called in post-order traversal.
LayoutRect RenderLayerBacking::computeParentGraphicsLayerRect(RenderLayer* compositedAncestor, LayoutSize& ancestorClippingLayerOffset) const
{
    if (!compositedAncestor || !compositedAncestor->backing())
        return renderer().view().documentRect();

    auto* ancestorBackingLayer = compositedAncestor->backing();
    LayoutRect parentGraphicsLayerRect;
    if (m_owningLayer.isInsideFragmentedFlow()) {
        // FIXME: flows/columns need work.
        LayoutRect ancestorCompositedBounds = ancestorBackingLayer->compositedBounds();
        ancestorCompositedBounds.setLocation(LayoutPoint());
        parentGraphicsLayerRect = ancestorCompositedBounds;
    }

    if (ancestorBackingLayer->hasClippingLayer()) {
        // If the compositing ancestor has a layer to clip children, we parent in that, and therefore position relative to it.
        LayoutRect clippingBox = clipBox(downcast<RenderBox>(compositedAncestor->renderer()));
        LayoutSize clippingBoxOffset = computeOffsetFromAncestorGraphicsLayer(compositedAncestor, clippingBox.location(), deviceScaleFactor());
        parentGraphicsLayerRect = snappedGraphicsLayer(clippingBoxOffset, clippingBox.size(), deviceScaleFactor()).m_snappedRect;
    }

    if (compositedAncestor->hasCompositedScrollableOverflow()) {
        LayoutRect ancestorCompositedBounds = ancestorBackingLayer->compositedBounds();
        auto& renderBox = downcast<RenderBox>(compositedAncestor->renderer());
        LayoutRect paddingBoxIncludingScrollbar = renderBox.paddingBoxRectIncludingScrollbar();
        ScrollOffset scrollOffset = compositedAncestor->scrollOffset();
        parentGraphicsLayerRect = LayoutRect((paddingBoxIncludingScrollbar.location() - toLayoutSize(ancestorCompositedBounds.location()) - toLayoutSize(scrollOffset)), paddingBoxIncludingScrollbar.size());
    }

    if (m_ancestorClippingLayer) {
        // Call calculateRects to get the backgroundRect which is what is used to clip the contents of this
        // layer. Note that we call it with temporaryClipRects = true because normally when computing clip rects
        // for a compositing layer, rootLayer is the layer itself.
        ShouldRespectOverflowClip shouldRespectOverflowClip = compositedAncestor->isolatesCompositedBlending() ? RespectOverflowClip : IgnoreOverflowClip;
        RenderLayer::ClipRectsContext clipRectsContext(compositedAncestor, TemporaryClipRects, IgnoreOverlayScrollbarSize, shouldRespectOverflowClip);
        LayoutRect parentClipRect = m_owningLayer.backgroundClipRect(clipRectsContext).rect(); // FIXME: Incorrect for CSS regions.
        ASSERT(!parentClipRect.isInfinite());
        LayoutSize clippingOffset = computeOffsetFromAncestorGraphicsLayer(compositedAncestor, parentClipRect.location(), deviceScaleFactor());
        LayoutRect snappedClippingLayerRect = snappedGraphicsLayer(clippingOffset, parentClipRect.size(), deviceScaleFactor()).m_snappedRect;
        // The primary layer is then parented in, and positioned relative to this clipping layer.
        ancestorClippingLayerOffset = snappedClippingLayerRect.location() - parentGraphicsLayerRect.location();
        parentGraphicsLayerRect = snappedClippingLayerRect;
    }
    return parentGraphicsLayerRect;
}

void RenderLayerBacking::updateGeometry()
{
    ASSERT(!m_owningLayer.normalFlowListDirty());
    ASSERT(!m_owningLayer.zOrderListsDirty());
    ASSERT(!m_owningLayer.descendantDependentFlagsAreDirty());
    ASSERT(!renderer().view().needsLayout());

    const RenderStyle& style = renderer().style();

    bool isRunningAcceleratedTransformAnimation = false;
    bool isRunningAcceleratedOpacityAnimation = false;
    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
        if (auto* timeline = renderer().documentTimeline()) {
            isRunningAcceleratedTransformAnimation = timeline->isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyTransform);
            isRunningAcceleratedOpacityAnimation = timeline->isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyOpacity);
        }
    } else {
        isRunningAcceleratedTransformAnimation = renderer().animation().isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyTransform);
        isRunningAcceleratedOpacityAnimation = renderer().animation().isRunningAcceleratedAnimationOnRenderer(renderer(), CSSPropertyOpacity);
    }

    // Set transform property, if it is not animating. We have to do this here because the transform
    // is affected by the layer dimensions.
    if (!isRunningAcceleratedTransformAnimation)
        updateTransform(style);

    // Set opacity, if it is not animating.
    if (!isRunningAcceleratedOpacityAnimation)
        updateOpacity(style);

    updateFilters(style);
#if ENABLE(FILTERS_LEVEL_2)
    updateBackdropFilters(style);
#endif
#if ENABLE(CSS_COMPOSITING)
    updateBlendMode(style);
#endif

    // FIXME: reflections should force transform-style to be flat in the style: https://bugs.webkit.org/show_bug.cgi?id=106959
    bool preserves3D = style.transformStyle3D() == TransformStyle3D::Preserve3D && !renderer().hasReflection();
    m_graphicsLayer->setPreserves3D(preserves3D);
    m_graphicsLayer->setBackfaceVisibility(style.backfaceVisibility() == BackfaceVisibility::Visible);

    auto* compositedAncestor = m_owningLayer.ancestorCompositingLayer();
    LayoutSize ancestorClippingLayerOffset;
    LayoutRect parentGraphicsLayerRect = computeParentGraphicsLayerRect(compositedAncestor, ancestorClippingLayerOffset);
    LayoutRect primaryGraphicsLayerRect = computePrimaryGraphicsLayerRect(parentGraphicsLayerRect);

    ComputedOffsets compositedBoundsOffset(m_owningLayer, compositedBounds(), parentGraphicsLayerRect, primaryGraphicsLayerRect);
    m_compositedBoundsOffsetFromGraphicsLayer = compositedBoundsOffset.fromPrimaryGraphicsLayer();
    m_graphicsLayer->setPosition(primaryGraphicsLayerRect.location());
    m_graphicsLayer->setSize(primaryGraphicsLayerRect.size());

    auto computeAnimationExtent = [&] () -> Optional<FloatRect> {
        LayoutRect animatedBounds;
        if (isRunningAcceleratedTransformAnimation && m_owningLayer.getOverlapBoundsIncludingChildrenAccountingForTransformAnimations(animatedBounds, RenderLayer::IncludeCompositedDescendants))
            return FloatRect(animatedBounds);
        return { };
    };
    m_graphicsLayer->setAnimationExtent(computeAnimationExtent());

    ComputedOffsets rendererOffset(m_owningLayer, LayoutRect(), parentGraphicsLayerRect, primaryGraphicsLayerRect);
    if (m_ancestorClippingLayer) {
        // Clipping layer is parented in the ancestor layer.
        m_ancestorClippingLayer->setPosition(toLayoutPoint(ancestorClippingLayerOffset));
        m_ancestorClippingLayer->setSize(parentGraphicsLayerRect.size());
        m_ancestorClippingLayer->setOffsetFromRenderer(-rendererOffset.fromParentGraphicsLayer());
    }

    if (m_contentsContainmentLayer) {
        m_contentsContainmentLayer->setPreserves3D(preserves3D);
        m_contentsContainmentLayer->setPosition(primaryGraphicsLayerRect.location());
        m_graphicsLayer->setPosition(FloatPoint());
        // Use the same size as m_graphicsLayer so transforms behave correctly.
        m_contentsContainmentLayer->setSize(primaryGraphicsLayerRect.size());
    }

    // Compute renderer offset from primary graphics layer. Note that primaryGraphicsLayerRect is in parentGraphicsLayer's coordinate system which is not necessarily
    // the same as the ancestor graphics layer.
    OffsetFromRenderer primaryGraphicsLayerOffsetFromRenderer;
    LayoutSize oldSubpixelOffsetFromRenderer = m_subpixelOffsetFromRenderer;
    primaryGraphicsLayerOffsetFromRenderer = computeOffsetFromRenderer(-rendererOffset.fromPrimaryGraphicsLayer(), deviceScaleFactor());
    m_subpixelOffsetFromRenderer = primaryGraphicsLayerOffsetFromRenderer.m_subpixelOffset;

    if (primaryGraphicsLayerOffsetFromRenderer.m_devicePixelOffset != m_graphicsLayer->offsetFromRenderer()) {
        m_graphicsLayer->setOffsetFromRenderer(primaryGraphicsLayerOffsetFromRenderer.m_devicePixelOffset);
        positionOverflowControlsLayers();
    }

    if (!m_isMainFrameRenderViewLayer && !m_isFrameLayerWithTiledBacking && !m_requiresBackgroundLayer) {
        // For non-root layers, background is always painted by the primary graphics layer.
        ASSERT(!m_backgroundLayer);
        // Subpixel offset from graphics layer or size changed.
        bool hadSubpixelRounding = !m_subpixelOffsetFromRenderer.isZero() || compositedBounds().size() != primaryGraphicsLayerRect.size();
        m_graphicsLayer->setContentsOpaque(!hadSubpixelRounding && m_owningLayer.backgroundIsKnownToBeOpaqueInRect(compositedBounds()));
    }

    // If we have a layer that clips children, position it.
    LayoutRect clippingBox;
    if (auto* clipLayer = clippingLayer()) {
        clippingBox = clipBox(downcast<RenderBox>(renderer()));
        // Clipping layer is parented in the primary graphics layer.
        LayoutSize clipBoxOffsetFromGraphicsLayer = toLayoutSize(clippingBox.location()) + rendererOffset.fromPrimaryGraphicsLayer();
        SnappedRectInfo snappedClippingGraphicsLayer = snappedGraphicsLayer(clipBoxOffsetFromGraphicsLayer, clippingBox.size(), deviceScaleFactor());
        clipLayer->setPosition(snappedClippingGraphicsLayer.m_snappedRect.location());
        clipLayer->setSize(snappedClippingGraphicsLayer.m_snappedRect.size());
        clipLayer->setOffsetFromRenderer(toLayoutSize(clippingBox.location() - snappedClippingGraphicsLayer.m_snapDelta));

        if (m_childClippingMaskLayer && !m_scrollingLayer) {
            m_childClippingMaskLayer->setSize(clipLayer->size());
            m_childClippingMaskLayer->setPosition(FloatPoint());
            m_childClippingMaskLayer->setOffsetFromRenderer(clipLayer->offsetFromRenderer());
        }
    }
    
    if (m_maskLayer)
        updateMaskingLayerGeometry();
    
    if (renderer().hasTransformRelatedProperty()) {
        // Update properties that depend on layer dimensions.
        FloatPoint3D transformOrigin = computeTransformOriginForPainting(downcast<RenderBox>(renderer()).borderBoxRect());
        FloatPoint layerOffset = roundPointToDevicePixels(toLayoutPoint(rendererOffset.fromParentGraphicsLayer()), deviceScaleFactor());
        // Compute the anchor point, which is in the center of the renderer box unless transform-origin is set.
        FloatPoint3D anchor(
            primaryGraphicsLayerRect.width() ? ((layerOffset.x() - primaryGraphicsLayerRect.x()) + transformOrigin.x()) / primaryGraphicsLayerRect.width() : 0.5,
            primaryGraphicsLayerRect.height() ? ((layerOffset.y() - primaryGraphicsLayerRect.y())+ transformOrigin.y()) / primaryGraphicsLayerRect.height() : 0.5,
            transformOrigin.z());

        if (m_contentsContainmentLayer)
            m_contentsContainmentLayer->setAnchorPoint(anchor);
        else
            m_graphicsLayer->setAnchorPoint(anchor);

        auto* clipLayer = clippingLayer();
        if (style.hasPerspective()) {
            TransformationMatrix t = owningLayer().perspectiveTransform();
            
            if (clipLayer) {
                clipLayer->setChildrenTransform(t);
                m_graphicsLayer->setChildrenTransform(TransformationMatrix());
            }
            else
                m_graphicsLayer->setChildrenTransform(t);
        } else {
            if (clipLayer)
                clipLayer->setChildrenTransform(TransformationMatrix());
            else
                m_graphicsLayer->setChildrenTransform(TransformationMatrix());
        }
    } else {
        m_graphicsLayer->setAnchorPoint(FloatPoint3D(0.5, 0.5, 0));
        if (m_contentsContainmentLayer)
            m_contentsContainmentLayer->setAnchorPoint(FloatPoint3D(0.5, 0.5, 0));
    }

    if (m_foregroundLayer) {
        FloatPoint foregroundPosition;
        FloatSize foregroundSize = primaryGraphicsLayerRect.size();
        FloatSize foregroundOffset = m_graphicsLayer->offsetFromRenderer();
        if (hasClippingLayer()) {
            // If we have a clipping layer (which clips descendants), then the foreground layer is a child of it,
            // so that it gets correctly sorted with children. In that case, position relative to the clipping layer.
            foregroundSize = FloatSize(clippingBox.size());
            foregroundOffset = toFloatSize(clippingBox.location());
        }

        m_foregroundLayer->setPosition(foregroundPosition);
        m_foregroundLayer->setSize(foregroundSize);
        m_foregroundLayer->setOffsetFromRenderer(foregroundOffset);
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

    if (m_owningLayer.reflectionLayer() && m_owningLayer.reflectionLayer()->isComposited()) {
        auto* reflectionBacking = m_owningLayer.reflectionLayer()->backing();
        reflectionBacking->updateGeometry();
        
        // The reflection layer has the bounds of m_owningLayer.reflectionLayer(),
        // but the reflected layer is the bounds of this layer, so we need to position it appropriately.
        FloatRect layerBounds = this->compositedBounds();
        FloatRect reflectionLayerBounds = reflectionBacking->compositedBounds();
        reflectionBacking->graphicsLayer()->setReplicatedLayerPosition(FloatPoint(layerBounds.location() - reflectionLayerBounds.location()));
    }

    if (m_scrollingLayer) {
        ASSERT(m_scrollingContentsLayer);
        auto& renderBox = downcast<RenderBox>(renderer());
        LayoutRect paddingBoxIncludingScrollbar = renderBox.paddingBoxRectIncludingScrollbar();
        ScrollOffset scrollOffset = m_owningLayer.scrollOffset();

        // FIXME: need to do some pixel snapping here.
        m_scrollingLayer->setPosition(FloatPoint(paddingBoxIncludingScrollbar.location() - compositedBounds().location()));

        m_scrollingLayer->setSize(roundedIntSize(LayoutSize(renderBox.clientWidth(), renderBox.clientHeight())));
#if PLATFORM(IOS_FAMILY)
        FloatSize oldScrollingLayerOffset = m_scrollingLayer->offsetFromRenderer();
        m_scrollingLayer->setOffsetFromRenderer(FloatPoint() - paddingBoxIncludingScrollbar.location());
        bool paddingBoxOffsetChanged = oldScrollingLayerOffset != m_scrollingLayer->offsetFromRenderer();

        if (m_owningLayer.isInUserScroll()) {
            // If scrolling is happening externally, we don't want to touch the layer bounds origin here because that will cause jitter.
            m_scrollingLayer->syncBoundsOrigin(scrollOffset);
            m_owningLayer.setRequiresScrollBoundsOriginUpdate(true);
        } else {
            // Note that we implement the contents offset via the bounds origin on this layer, rather than a position on the sublayer.
            m_scrollingLayer->setBoundsOrigin(scrollOffset);
            m_owningLayer.setRequiresScrollBoundsOriginUpdate(false);
        }
        
        IntSize scrollSize(m_owningLayer.scrollWidth(), m_owningLayer.scrollHeight());

        m_scrollingContentsLayer->setPosition(FloatPoint());
        
        if (scrollSize != m_scrollingContentsLayer->size() || paddingBoxOffsetChanged)
            m_scrollingContentsLayer->setNeedsDisplay();

        m_scrollingContentsLayer->setSize(scrollSize);
        // Scrolling the content layer does not need to trigger a repaint. The offset will be compensated away during painting.
        m_scrollingContentsLayer->setScrollOffset(scrollOffset, GraphicsLayer::DontSetNeedsDisplay);
        m_scrollingContentsLayer->setOffsetFromRenderer(toLayoutSize(paddingBoxIncludingScrollbar.location()), GraphicsLayer::DontSetNeedsDisplay);
#else
        m_scrollingContentsLayer->setPosition(-scrollOffset);

        FloatSize oldScrollingLayerOffset = m_scrollingLayer->offsetFromRenderer();
        m_scrollingLayer->setOffsetFromRenderer(-toFloatSize(paddingBoxIncludingScrollbar.location()));

        if (m_childClippingMaskLayer) {
            m_childClippingMaskLayer->setPosition(m_scrollingLayer->position());
            m_childClippingMaskLayer->setSize(m_scrollingLayer->size());
            m_childClippingMaskLayer->setOffsetFromRenderer(toFloatSize(paddingBoxIncludingScrollbar.location()));
        }

        bool paddingBoxOffsetChanged = oldScrollingLayerOffset != m_scrollingLayer->offsetFromRenderer();

        IntSize scrollSize(m_owningLayer.scrollWidth(), m_owningLayer.scrollHeight());
        if (scrollSize != m_scrollingContentsLayer->size() || paddingBoxOffsetChanged)
            m_scrollingContentsLayer->setNeedsDisplay();

        m_scrollingContentsLayer->setSize(scrollSize);
        m_scrollingContentsLayer->setScrollOffset(scrollOffset, GraphicsLayer::DontSetNeedsDisplay);
        m_scrollingContentsLayer->setOffsetFromRenderer(toLayoutSize(paddingBoxIncludingScrollbar.location()), GraphicsLayer::DontSetNeedsDisplay);
#endif

        if (m_foregroundLayer) {
            m_foregroundLayer->setSize(m_scrollingContentsLayer->size());
            m_foregroundLayer->setOffsetFromRenderer(m_scrollingContentsLayer->offsetFromRenderer() - toLayoutSize(m_scrollingContentsLayer->scrollOffset()));
        }
    }

    // If this layer was created just for clipping or to apply perspective, it doesn't need its own backing store.
    LayoutRect ancestorCompositedBounds = compositedAncestor ? compositedAncestor->backing()->compositedBounds() : LayoutRect();
    setRequiresOwnBackingStore(compositor().requiresOwnBackingStore(m_owningLayer, compositedAncestor,
        LayoutRect(toLayoutPoint(compositedBoundsOffset.fromParentGraphicsLayer()), compositedBounds().size()), ancestorCompositedBounds));
#if ENABLE(FILTERS_LEVEL_2)
    updateBackdropFiltersGeometry();
#endif
    updateAfterWidgetResize();

    if (subpixelOffsetFromRendererChanged(oldSubpixelOffsetFromRenderer, m_subpixelOffsetFromRenderer, deviceScaleFactor()) && canIssueSetNeedsDisplay())
        setContentsNeedDisplay();

    compositor().updateScrollCoordinatedStatus(m_owningLayer, { RenderLayerCompositor::ScrollingNodeChangeFlags::Layer, RenderLayerCompositor::ScrollingNodeChangeFlags::LayerGeometry });
}

void RenderLayerBacking::updateAfterDescendants()
{
    // FIXME: this potentially duplicates work we did in updateConfiguration().
    PaintedContentsInfo contentsInfo(*this);
    contentsInfo.setWantsSubpixelAntialiasedTextState(GraphicsLayer::supportsSubpixelAntialiasedLayerText() && FontCascade::isSubpixelAntialiasingAvailable());

    if (!m_owningLayer.isRenderViewLayer()) {
        bool didUpdateContentsRect = false;
        updateDirectlyCompositedBoxDecorations(contentsInfo, didUpdateContentsRect);
        if (!didUpdateContentsRect && m_graphicsLayer->usesContentsLayer())
            resetContentsRect();
    }

    updateDrawsContent(contentsInfo);

    m_graphicsLayer->setContentsVisible(m_owningLayer.hasVisibleContent() || hasVisibleNonCompositedDescendants());
    if (m_scrollingLayer) {
        m_scrollingLayer->setContentsVisible(renderer().style().visibility() == Visibility::Visible);
        m_scrollingLayer->setUserInteractionEnabled(renderer().style().pointerEvents() != PointerEvents::None);
    }
}

// FIXME: Avoid repaints when clip path changes.
void RenderLayerBacking::updateMaskingLayerGeometry()
{
    m_maskLayer->setSize(m_graphicsLayer->size());
    m_maskLayer->setPosition(FloatPoint());
    m_maskLayer->setOffsetFromRenderer(m_graphicsLayer->offsetFromRenderer());
    
    if (!m_maskLayer->drawsContent()) {
        if (renderer().hasClipPath()) {
            ASSERT(renderer().style().clipPath()->type() != ClipPathOperation::Reference);

            WindRule windRule;
            // FIXME: Use correct reference box for inlines: https://bugs.webkit.org/show_bug.cgi?id=129047
            LayoutRect boundingBox = m_owningLayer.boundingBox(&m_owningLayer);
            LayoutRect referenceBoxForClippedInline = LayoutRect(snapRectToDevicePixels(boundingBox, deviceScaleFactor()));
            LayoutSize offset = LayoutSize(snapSizeToDevicePixel(-m_subpixelOffsetFromRenderer, LayoutPoint(), deviceScaleFactor()));
            Path clipPath = m_owningLayer.computeClipPath(offset, referenceBoxForClippedInline, windRule);

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
    if (m_ancestorClippingLayer)
        m_ancestorClippingLayer->removeAllChildren();
    
    if (m_contentsContainmentLayer) {
        m_contentsContainmentLayer->removeAllChildren();
        if (m_ancestorClippingLayer)
            m_ancestorClippingLayer->addChild(*m_contentsContainmentLayer);
    }
    
    if (m_backgroundLayer)
        m_contentsContainmentLayer->addChild(*m_backgroundLayer);

    if (m_contentsContainmentLayer)
        m_contentsContainmentLayer->addChild(*m_graphicsLayer);
    else if (m_ancestorClippingLayer)
        m_ancestorClippingLayer->addChild(*m_graphicsLayer);

    if (m_childContainmentLayer)
        m_graphicsLayer->addChild(*m_childContainmentLayer);

    if (m_scrollingLayer) {
        auto* superlayer = m_childContainmentLayer ? m_childContainmentLayer.get() : m_graphicsLayer.get();
        superlayer->addChild(*m_scrollingLayer);
    }

    // The clip for child layers does not include space for overflow controls, so they exist as
    // siblings of the clipping layer if we have one. Normal children of this layer are set as
    // children of the clipping layer.
    if (m_layerForHorizontalScrollbar)
        m_graphicsLayer->addChild(*m_layerForHorizontalScrollbar);

    if (m_layerForVerticalScrollbar)
        m_graphicsLayer->addChild(*m_layerForVerticalScrollbar);

    if (m_layerForScrollCorner)
        m_graphicsLayer->addChild(*m_layerForScrollCorner);
}

void RenderLayerBacking::resetContentsRect()
{
    m_graphicsLayer->setContentsRect(snapRectToDevicePixels(contentsBox(), deviceScaleFactor()));
    
    if (is<RenderBox>(renderer())) {
        LayoutRect boxRect(LayoutPoint(), downcast<RenderBox>(renderer()).size());
        boxRect.move(contentOffsetInCompostingLayer());
        FloatRoundedRect contentsClippingRect = renderer().style().getRoundedInnerBorderFor(boxRect).pixelSnappedRoundedRectForPainting(deviceScaleFactor());
        m_graphicsLayer->setContentsClippingRect(contentsClippingRect);
    }

    m_graphicsLayer->setContentsTileSize(IntSize());
    m_graphicsLayer->setContentsTilePhase(IntSize());
}

void RenderLayerBacking::updateDrawsContent()
{
    PaintedContentsInfo contentsInfo(*this);
    contentsInfo.setWantsSubpixelAntialiasedTextState(GraphicsLayer::supportsSubpixelAntialiasedLayerText());

    updateDrawsContent(contentsInfo);
}

void RenderLayerBacking::updateDrawsContent(PaintedContentsInfo& contentsInfo)
{
    if (m_scrollingLayer) {
        // We don't have to consider overflow controls, because we know that the scrollbars are drawn elsewhere.
        // m_graphicsLayer only needs backing store if the non-scrolling parts (background, outlines, borders, shadows etc) need to paint.
        // m_scrollingLayer never has backing store.
        // m_scrollingContentsLayer only needs backing store if the scrolled contents need to paint.
        bool hasNonScrollingPaintedContent = m_owningLayer.hasVisibleContent() && m_owningLayer.hasVisibleBoxDecorationsOrBackground();
        m_graphicsLayer->setDrawsContent(hasNonScrollingPaintedContent);

        bool hasScrollingPaintedContent = m_owningLayer.hasVisibleContent() && (renderer().hasBackground() || contentsInfo.paintsContent());
        m_scrollingContentsLayer->setDrawsContent(hasScrollingPaintedContent);
        return;
    }

    bool hasPaintedContent = containsPaintedContent(contentsInfo);

    m_paintsSubpixelAntialiasedText = renderer().settings().subpixelAntialiasedLayerTextEnabled() && contentsInfo.paintsSubpixelAntialiasedText();

    // FIXME: we could refine this to only allocate backing for one of these layers if possible.
    m_graphicsLayer->setDrawsContent(hasPaintedContent);
    if (m_foregroundLayer) {
        m_foregroundLayer->setDrawsContent(hasPaintedContent);
        m_foregroundLayer->setSupportsSubpixelAntialiasedText(m_paintsSubpixelAntialiasedText);
        // The text content is painted into the foreground layer.
        // FIXME: this ignores SVG background images which may contain text.
        m_graphicsLayer->setSupportsSubpixelAntialiasedText(false);
    } else
        m_graphicsLayer->setSupportsSubpixelAntialiasedText(m_paintsSubpixelAntialiasedText);

    if (m_backgroundLayer)
        m_backgroundLayer->setDrawsContent(m_backgroundLayerPaintsFixedRootBackground ? hasPaintedContent : contentsInfo.paintsBoxDecorations());
}

// Return true if the layer changed.
bool RenderLayerBacking::updateAncestorClippingLayer(bool needsAncestorClip)
{
    bool layersChanged = false;

    if (needsAncestorClip) {
        if (!m_ancestorClippingLayer) {
            m_ancestorClippingLayer = createGraphicsLayer("ancestor clipping");
            m_ancestorClippingLayer->setMasksToBounds(true);
            layersChanged = true;
        }
    } else if (hasAncestorClippingLayer()) {
        willDestroyLayer(m_ancestorClippingLayer.get());
        GraphicsLayer::unparentAndClear(m_ancestorClippingLayer);
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
            m_childContainmentLayer = createGraphicsLayer("child clipping");
            m_childContainmentLayer->setMasksToBounds(true);
            layersChanged = true;
        }
    } else if (hasClippingLayer()) {
        willDestroyLayer(m_childContainmentLayer.get());
        GraphicsLayer::unparentAndClear(m_childContainmentLayer);
        layersChanged = true;
    }
    
    return layersChanged;
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

bool RenderLayerBacking::requiresHorizontalScrollbarLayer() const
{
    if (!m_owningLayer.hasOverlayScrollbars())
        return false;
    return m_owningLayer.horizontalScrollbar();
}

bool RenderLayerBacking::requiresVerticalScrollbarLayer() const
{
    if (!m_owningLayer.hasOverlayScrollbars())
        return false;
    return m_owningLayer.verticalScrollbar();
}

bool RenderLayerBacking::requiresScrollCornerLayer() const
{
    if (!m_owningLayer.hasOverlayScrollbars())
        return false;
    return !m_owningLayer.scrollCornerAndResizerRect().isEmpty();
}

bool RenderLayerBacking::updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer)
{
    bool horizontalScrollbarLayerChanged = false;
    if (needsHorizontalScrollbarLayer) {
        if (!m_layerForHorizontalScrollbar) {
            m_layerForHorizontalScrollbar = createGraphicsLayer("horizontal scrollbar");
            m_layerForHorizontalScrollbar->setCanDetachBackingStore(false);
            horizontalScrollbarLayerChanged = true;
        }
    } else if (m_layerForHorizontalScrollbar) {
        willDestroyLayer(m_layerForHorizontalScrollbar.get());
        GraphicsLayer::unparentAndClear(m_layerForHorizontalScrollbar);
        horizontalScrollbarLayerChanged = true;
    }

    bool verticalScrollbarLayerChanged = false;
    if (needsVerticalScrollbarLayer) {
        if (!m_layerForVerticalScrollbar) {
            m_layerForVerticalScrollbar = createGraphicsLayer("vertical scrollbar");
            m_layerForVerticalScrollbar->setCanDetachBackingStore(false);
            verticalScrollbarLayerChanged = true;
        }
    } else if (m_layerForVerticalScrollbar) {
        willDestroyLayer(m_layerForVerticalScrollbar.get());
        GraphicsLayer::unparentAndClear(m_layerForVerticalScrollbar);
        verticalScrollbarLayerChanged = true;
    }

    bool scrollCornerLayerChanged = false;
    if (needsScrollCornerLayer) {
        if (!m_layerForScrollCorner) {
            m_layerForScrollCorner = createGraphicsLayer("scroll corner");
            m_layerForScrollCorner->setCanDetachBackingStore(false);
            scrollCornerLayerChanged = true;
        }
    } else if (m_layerForScrollCorner) {
        willDestroyLayer(m_layerForScrollCorner.get());
        GraphicsLayer::unparentAndClear(m_layerForScrollCorner);
        scrollCornerLayerChanged = true;
    }

    if (auto* scrollingCoordinator = m_owningLayer.page().scrollingCoordinator()) {
        if (horizontalScrollbarLayerChanged)
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_owningLayer, HorizontalScrollbar);
        if (verticalScrollbarLayerChanged)
            scrollingCoordinator->scrollableAreaScrollbarLayerDidChange(m_owningLayer, VerticalScrollbar);
    }

    return horizontalScrollbarLayerChanged || verticalScrollbarLayerChanged || scrollCornerLayerChanged;
}

void RenderLayerBacking::positionOverflowControlsLayers()
{
    if (!m_owningLayer.hasScrollbars())
        return;

    const IntRect borderBox = snappedIntRect(renderBox()->borderBoxRect());

    FloatSize offsetFromRenderer = m_graphicsLayer->offsetFromRenderer();
    if (auto* layer = layerForHorizontalScrollbar()) {
        IntRect hBarRect = m_owningLayer.rectForHorizontalScrollbar(borderBox);
        layer->setPosition(hBarRect.location() - offsetFromRenderer);
        layer->setSize(hBarRect.size());
        if (layer->usesContentsLayer()) {
            IntRect barRect = IntRect(IntPoint(), hBarRect.size());
            layer->setContentsRect(barRect);
            layer->setContentsClippingRect(FloatRoundedRect(barRect));
        }
        layer->setDrawsContent(m_owningLayer.horizontalScrollbar() && !layer->usesContentsLayer());
    }
    
    if (auto* layer = layerForVerticalScrollbar()) {
        IntRect vBarRect = m_owningLayer.rectForVerticalScrollbar(borderBox);
        layer->setPosition(vBarRect.location() - offsetFromRenderer);
        layer->setSize(vBarRect.size());
        if (layer->usesContentsLayer()) {
            IntRect barRect = IntRect(IntPoint(), vBarRect.size());
            layer->setContentsRect(barRect);
            layer->setContentsClippingRect(FloatRoundedRect(barRect));
        }
        layer->setDrawsContent(m_owningLayer.verticalScrollbar() && !layer->usesContentsLayer());
    }

    if (auto* layer = layerForScrollCorner()) {
        const LayoutRect& scrollCornerAndResizer = m_owningLayer.scrollCornerAndResizerRect();
        layer->setPosition(scrollCornerAndResizer.location() - offsetFromRenderer);
        layer->setSize(scrollCornerAndResizer.size());
        layer->setDrawsContent(!scrollCornerAndResizer.isEmpty());
    }
}

bool RenderLayerBacking::updateForegroundLayer(bool needsForegroundLayer)
{
    bool layerChanged = false;
    if (needsForegroundLayer) {
        if (!m_foregroundLayer) {
            String layerName = m_owningLayer.name() + " (foreground)";
            m_foregroundLayer = createGraphicsLayer(layerName);
            m_foregroundLayer->setDrawsContent(true);
            m_foregroundLayer->setPaintingPhase(GraphicsLayerPaintForeground);
            layerChanged = true;
        }
    } else if (m_foregroundLayer) {
        willDestroyLayer(m_foregroundLayer.get());
        GraphicsLayer::unparentAndClear(m_foregroundLayer);
        layerChanged = true;
    }

    if (layerChanged) {
        m_graphicsLayer->setNeedsDisplay();
        m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());
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
            m_backgroundLayer->setPaintingPhase(GraphicsLayerPaintBackground);
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
    
    if (layerChanged) {
        m_graphicsLayer->setNeedsDisplay();

        if (m_backgroundLayerPaintsFixedRootBackground)
            compositor().fixedRootBackgroundLayerChanged();
    }
    
    return layerChanged;
}

// Masking layer is used for masks or clip-path.
void RenderLayerBacking::updateMaskingLayer(bool hasMask, bool hasClipPath)
{
    bool layerChanged = false;
    if (hasMask || hasClipPath) {
        GraphicsLayerPaintingPhase maskPhases = 0;
        if (hasMask)
            maskPhases = GraphicsLayerPaintMask;
        
        if (hasClipPath) {
            // If we have a mask, we need to paint the combined clip-path and mask into the mask layer.
            if (hasMask || renderer().style().clipPath()->type() == ClipPathOperation::Reference || !GraphicsLayer::supportsLayerType(GraphicsLayer::Type::Shape))
                maskPhases |= GraphicsLayerPaintClipPath;
        }

        bool paintsContent = maskPhases;
        GraphicsLayer::Type requiredLayerType = paintsContent ? GraphicsLayer::Type::Normal : GraphicsLayer::Type::Shape;
        if (m_maskLayer && m_maskLayer->type() != requiredLayerType) {
            m_graphicsLayer->setMaskLayer(nullptr);
            willDestroyLayer(m_maskLayer.get());
            GraphicsLayer::clear(m_maskLayer);
        }

        if (!m_maskLayer) {
            m_maskLayer = createGraphicsLayer("mask", requiredLayerType);
            m_maskLayer->setDrawsContent(paintsContent);
            m_maskLayer->setPaintingPhase(maskPhases);
            layerChanged = true;
            m_graphicsLayer->setMaskLayer(m_maskLayer.copyRef());
            // We need a geometry update to size the new mask layer.
            m_owningLayer.setNeedsCompositingGeometryUpdate();
        }
    } else if (m_maskLayer) {
        m_graphicsLayer->setMaskLayer(nullptr);
        willDestroyLayer(m_maskLayer.get());
        GraphicsLayer::clear(m_maskLayer);
        layerChanged = true;
    }

    if (layerChanged)
        m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());
}

void RenderLayerBacking::updateChildClippingStrategy(bool needsDescendantsClippingLayer)
{
    if (hasClippingLayer() && needsDescendantsClippingLayer) {
        if (is<RenderBox>(renderer()) && (renderer().style().clipPath() || renderer().style().hasBorderRadius())) {
            // FIXME: we shouldn't get geometry here as layout may not have been udpated.
            LayoutRect boxRect(LayoutPoint(), downcast<RenderBox>(renderer()).size());
            boxRect.move(contentOffsetInCompostingLayer());
            FloatRoundedRect contentsClippingRect = renderer().style().getRoundedInnerBorderFor(boxRect).pixelSnappedRoundedRectForPainting(deviceScaleFactor());
            if (clippingLayer()->setMasksToBoundsRect(contentsClippingRect)) {
                clippingLayer()->setMaskLayer(nullptr);
                GraphicsLayer::clear(m_childClippingMaskLayer);
                return;
            }

            if (!m_childClippingMaskLayer) {
                m_childClippingMaskLayer = createGraphicsLayer("child clipping mask");
                m_childClippingMaskLayer->setDrawsContent(true);
                m_childClippingMaskLayer->setPaintingPhase(GraphicsLayerPaintChildClippingMask);
                clippingLayer()->setMaskLayer(m_childClippingMaskLayer.copyRef());
            }
        }
    } else {
        if (m_childClippingMaskLayer) {
            if (hasClippingLayer())
                clippingLayer()->setMaskLayer(nullptr);
            GraphicsLayer::clear(m_childClippingMaskLayer);
        } else 
            if (hasClippingLayer())
                clippingLayer()->setMasksToBoundsRect(FloatRoundedRect(FloatRect(FloatPoint(), clippingLayer()->size())));
    }
}

bool RenderLayerBacking::updateScrollingLayers(bool needsScrollingLayers)
{
    if (needsScrollingLayers == !!m_scrollingLayer)
        return false;

    if (!m_scrollingLayer) {
        // Outer layer which corresponds with the scroll view.
        m_scrollingLayer = createGraphicsLayer("scrolling container", GraphicsLayer::Type::Scrolling);
        m_scrollingLayer->setDrawsContent(false);
        m_scrollingLayer->setMasksToBounds(true);

        // Inner layer which renders the content that scrolls.
        m_scrollingContentsLayer = createGraphicsLayer("scrolled Contents");
        m_scrollingContentsLayer->setDrawsContent(true);

        GraphicsLayerPaintingPhase paintPhase = GraphicsLayerPaintOverflowContents | GraphicsLayerPaintCompositedScroll;
        if (!m_foregroundLayer)
            paintPhase |= GraphicsLayerPaintForeground;
        m_scrollingContentsLayer->setPaintingPhase(paintPhase);
        m_scrollingLayer->addChild(*m_scrollingContentsLayer);
    } else {
        compositor().willRemoveScrollingLayerWithBacking(m_owningLayer, *this);

        willDestroyLayer(m_scrollingLayer.get());
        willDestroyLayer(m_scrollingContentsLayer.get());
        
        GraphicsLayer::unparentAndClear(m_scrollingLayer);
        GraphicsLayer::unparentAndClear(m_scrollingContentsLayer);
    }

    m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());
    m_graphicsLayer->setNeedsDisplay(); // Because painting phases changed.

    if (m_scrollingLayer)
        compositor().didAddScrollingLayer(m_owningLayer);
    
    return true;
}

void RenderLayerBacking::detachFromScrollingCoordinator(OptionSet<ScrollCoordinationRole> roles)
{
    if (!m_scrollingNodeID && !m_frameHostingNodeID && !m_viewportConstrainedNodeID)
        return;

    auto* scrollingCoordinator = m_owningLayer.page().scrollingCoordinator();
    if (!scrollingCoordinator)
        return;

    if (roles.contains(ScrollCoordinationRole::Scrolling) && m_scrollingNodeID) {
        LOG(Compositing, "Detaching Scrolling node %" PRIu64, m_scrollingNodeID);
        scrollingCoordinator->unparentChildrenAndDestroyNode(m_scrollingNodeID);
        m_scrollingNodeID = 0;
    }

    if (roles.contains(ScrollCoordinationRole::Scrolling) && m_frameHostingNodeID) {
        LOG(Compositing, "Detaching FrameHosting node %" PRIu64, m_frameHostingNodeID);
        scrollingCoordinator->unparentChildrenAndDestroyNode(m_frameHostingNodeID);
        m_frameHostingNodeID = 0;
    }

    if (roles.contains(ScrollCoordinationRole::ViewportConstrained) && m_viewportConstrainedNodeID) {
        LOG(Compositing, "Detaching ViewportConstrained node %" PRIu64, m_viewportConstrainedNodeID);
        scrollingCoordinator->unparentChildrenAndDestroyNode(m_viewportConstrainedNodeID);
        m_viewportConstrainedNodeID = 0;
    }
}

void RenderLayerBacking::setIsScrollCoordinatedWithViewportConstrainedRole(bool viewportCoordinated)
{
    m_graphicsLayer->setIsViewportConstrained(viewportCoordinated);
}

GraphicsLayerPaintingPhase RenderLayerBacking::paintingPhaseForPrimaryLayer() const
{
    unsigned phase = 0;
    if (!m_backgroundLayer)
        phase |= GraphicsLayerPaintBackground;
    if (!m_foregroundLayer)
        phase |= GraphicsLayerPaintForeground;

    if (m_scrollingContentsLayer) {
        phase &= ~GraphicsLayerPaintForeground;
        phase |= GraphicsLayerPaintCompositedScroll;
    }

    return static_cast<GraphicsLayerPaintingPhase>(phase);
}

float RenderLayerBacking::compositingOpacity(float rendererOpacity) const
{
    float finalOpacity = rendererOpacity;
    
    for (auto* curr = m_owningLayer.parent(); curr; curr = curr->parent()) {
        // We only care about parents that are stacking contexts.
        // Recall that opacity creates stacking context.
        if (!curr->isStackingContext())
            continue;
        
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
    return style.hasVisibleBorder() || style.hasBorderRadius() || style.hasOutline() || style.hasAppearance() || style.boxShadow() || style.hasFilter();
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
    if (!styleImage->isCachedImage())
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

    if (!contentsInfo.isSimpleContainer() || (is<RenderBox>(renderer()) && !downcast<RenderBox>(renderer()).paintsOwnBackground())) {
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
        m_graphicsLayer->setContentsToImage(0);
        return;
    }

    auto destRect = backgroundBoxForSimpleContainerPainting();
    FloatSize phase;
    FloatSize tileSize;
    // FIXME: Absolute paint location is required here.
    downcast<RenderBox>(renderer()).getGeometryForBackgroundImage(&renderer(), LayoutPoint(), destRect, phase, tileSize);

    m_graphicsLayer->setContentsTileSize(tileSize);
    m_graphicsLayer->setContentsTilePhase(phase);
    m_graphicsLayer->setContentsRect(destRect);
    m_graphicsLayer->setContentsClippingRect(FloatRoundedRect(destRect));
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

static bool supportsDirectlyCompositedBoxDecorations(const RenderLayerModelObject& renderer)
{
    if (!GraphicsLayer::supportsBackgroundColorContent())
        return false;

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

    // FIXME: we should be able to allow backgroundComposite; However since this is not a common use case it has been deferred for now.
    if (style.backgroundComposite() != CompositeSourceOver)
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

    if (request.hasPaintedContent == RequestState::Unknown)
        request.hasPaintedContent = RequestState::False;

    if (request.hasSubpixelAntialiasedText == RequestState::Unknown)
        request.hasSubpixelAntialiasedText = RequestState::False;

    return paintsContent;
}

static bool isRestartedPlugin(RenderObject& renderer)
{
    if (!is<RenderEmbeddedObject>(renderer))
        return false;

    auto& element = downcast<RenderEmbeddedObject>(renderer).frameOwnerElement();
    if (!is<HTMLPlugInElement>(element))
        return false;

    return downcast<HTMLPlugInElement>(element).isRestartedPlugin();
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
    if (renderer().isRenderReplaced() && (!isCompositedPlugin(renderer()) || isRestartedPlugin(renderer())))
        return false;

    if (renderer().isTextControl())
        return false;

    if (contentsInfo.paintsBoxDecorations() || contentsInfo.paintsContent())
        return false;

    if (renderer().style().backgroundClip() == FillBox::Text)
        return false;
    
    if (renderer().isDocumentElementRenderer() && m_owningLayer.isolatesCompositedBlending())
        return false;

    if (renderer().isRenderView()) {
        // Look to see if the root object has a non-simple background
        auto* rootObject = renderer().document().documentElement() ? renderer().document().documentElement()->renderer() : nullptr;
        if (!rootObject)
            return false;
        
        // Reject anything that has a border, a border-radius or outline,
        // or is not a simple background (no background, or solid color).
        if (hasPaintedBoxDecorationsOrBackgroundImage(rootObject->style()))
            return false;
        
        // Now look at the body's renderer.
        auto* body = renderer().document().body();
        if (!body)
            return false;
        auto* bodyRenderer = body->renderer();
        if (!bodyRenderer)
            return false;
        
        if (hasPaintedBoxDecorationsOrBackgroundImage(bodyRenderer->style()))
            return false;
    }

    return true;
}

// Returning true stops the traversal.
enum class LayerTraversal { Continue, Stop };

static LayerTraversal traverseVisibleNonCompositedDescendantLayers(RenderLayer& parent, const WTF::Function<LayerTraversal (const RenderLayer&)>& layerFunc)
{
    // FIXME: We shouldn't be called with a stale z-order lists. See bug 85512.
    parent.updateLayerListsIfNeeded();

#if !ASSERT_DISABLED
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

// Conservative test for having no rendered children.
bool RenderLayerBacking::isPaintDestinationForDescendantLayers(RenderLayer::PaintedContentRequest& request) const
{
    bool hasPaintingDescendant = false;
    traverseVisibleNonCompositedDescendantLayers(m_owningLayer, [&hasPaintingDescendant, &request](const RenderLayer& layer) {
        hasPaintingDescendant |= layer.isVisuallyNonEmpty(&request);
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

    // FIXME: we could optimize cases where the image, video or canvas is known to fill the border box entirely,
    // and set background color on the layer in that case, instead of allocating backing store and painting.
#if ENABLE(VIDEO)
    if (is<RenderVideo>(renderer()) && downcast<RenderVideo>(renderer()).shouldDisplayVideo())
        return m_owningLayer.hasVisibleBoxDecorationsOrBackground() || (!(downcast<RenderVideo>(renderer()).supportsAcceleratedRendering()) && m_requiresOwnBackingStore);
#endif

#if ENABLE(WEBGL) || ENABLE(ACCELERATED_2D_CANVAS)
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

        if (downcast<BitmapImage>(*image).orientationForCurrentFrame() != DefaultImageOrientation)
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

void RenderLayerBacking::contentChanged(ContentChangeType changeType)
{
    PaintedContentsInfo contentsInfo(*this);
    if ((changeType == ImageChanged) && contentsInfo.isDirectlyCompositedImage()) {
        updateImageContents(contentsInfo);
        return;
    }

    if ((changeType == BackgroundImageChanged) && canDirectlyCompositeBackgroundBackgroundImage(renderer().style()))
        m_owningLayer.setNeedsCompositingConfigurationUpdate();

    if ((changeType == MaskImageChanged) && m_maskLayer)
        m_owningLayer.setNeedsCompositingConfigurationUpdate();

#if ENABLE(WEBGL) || ENABLE(ACCELERATED_2D_CANVAS)
    if ((changeType == CanvasChanged || changeType == CanvasPixelsChanged) && renderer().isCanvas() && canvasCompositingStrategy(renderer()) == CanvasAsLayerContents) {
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

    // This is a no-op if the layer doesn't have an inner layer for the image.
    m_graphicsLayer->setContentsRect(snapRectToDevicePixels(contentsBox(), deviceScaleFactor()));

    LayoutRect boxRect(LayoutPoint(), imageRenderer.size());
    boxRect.move(contentOffsetInCompostingLayer());
    FloatRoundedRect contentsClippingRect = renderer().style().getRoundedInnerBorderFor(boxRect).pixelSnappedRoundedRectForPainting(deviceScaleFactor());
    m_graphicsLayer->setContentsClippingRect(contentsClippingRect);

    m_graphicsLayer->setContentsToImage(image);
    
    updateDrawsContent(contentsInfo);
    
    // Image animation is "lazy", in that it automatically stops unless someone is drawing
    // the image. So we have to kick the animation each time; this has the downside that the
    // image will keep animating, even if its layer is not visible.
    image->startAnimation();
}

FloatPoint3D RenderLayerBacking::computeTransformOriginForPainting(const LayoutRect& borderBox) const
{
    const RenderStyle& style = renderer().style();
    float deviceScaleFactor = this->deviceScaleFactor();

    FloatPoint3D origin;
    origin.setX(roundToDevicePixel(floatValueForLength(style.transformOriginX(), borderBox.width()), deviceScaleFactor));
    origin.setY(roundToDevicePixel(floatValueForLength(style.transformOriginY(), borderBox.height()), deviceScaleFactor));
    origin.setZ(style.transformOriginZ());

    return origin;
}

// Return the offset from the top-left of this compositing layer at which the renderer's contents are painted.
LayoutSize RenderLayerBacking::contentOffsetInCompostingLayer() const
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
    if (is<RenderReplaced>(renderBox)) {
        RenderReplaced& renderReplaced = downcast<RenderReplaced>(renderBox);
        contentsRect = renderReplaced.replacedContentRect();
    } else
        contentsRect = renderBox.contentBoxRect();

    contentsRect.move(contentOffsetInCompostingLayer());
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
    return LayoutRect();
}

FloatRect RenderLayerBacking::backgroundBoxForSimpleContainerPainting() const
{
    if (!is<RenderBox>(renderer()))
        return FloatRect();

    LayoutRect backgroundBox = backgroundRectForBox(downcast<RenderBox>(renderer()));
    backgroundBox.move(contentOffsetInCompostingLayer());
    return snapRectToDevicePixels(backgroundBox, deviceScaleFactor());
}

GraphicsLayer* RenderLayerBacking::parentForSublayers() const
{
    if (m_scrollingContentsLayer)
        return m_scrollingContentsLayer.get();

    return m_childContainmentLayer ? m_childContainmentLayer.get() : m_graphicsLayer.get();
}

GraphicsLayer* RenderLayerBacking::childForSuperlayers() const
{
    if (m_ancestorClippingLayer)
        return m_ancestorClippingLayer.get();

    if (m_contentsContainmentLayer)
        return m_contentsContainmentLayer.get();
    
    return m_graphicsLayer.get();
}

bool RenderLayerBacking::paintsIntoWindow() const
{
#if USE(COORDINATED_GRAPHICS_THREADED)
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

    if (m_childClippingMaskLayer && m_childClippingMaskLayer->drawsContent())
        m_childClippingMaskLayer->setNeedsDisplay();

    if (m_scrollingContentsLayer && m_scrollingContentsLayer->drawsContent())
        m_scrollingContentsLayer->setNeedsDisplay();
}

// r is in the coordinate space of the layer's render object
void RenderLayerBacking::setContentsNeedDisplayInRect(const LayoutRect& r, GraphicsLayer::ShouldClipToLayer shouldClip)
{
    ASSERT(!paintsIntoCompositedAncestor());
    
    // Use the repaint as a trigger to re-evaluate direct compositing (which is never used on the root layer).
    if (!m_owningLayer.isRenderViewLayer())
        m_owningLayer.setNeedsCompositingConfigurationUpdate();

    FloatRect pixelSnappedRectForPainting = snapRectToDevicePixels(r, deviceScaleFactor());
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

    if (m_childClippingMaskLayer && m_childClippingMaskLayer->drawsContent()) {
        FloatRect layerDirtyRect = r;
        layerDirtyRect.move(-m_childClippingMaskLayer->offsetFromRenderer());
        m_childClippingMaskLayer->setNeedsDisplayInRect(layerDirtyRect);
    }

    if (m_scrollingContentsLayer && m_scrollingContentsLayer->drawsContent()) {
        FloatRect layerDirtyRect = pixelSnappedRectForPainting;
        layerDirtyRect.move(-m_scrollingContentsLayer->offsetFromRenderer() + toLayoutSize(m_scrollingContentsLayer->scrollOffset()) - m_subpixelOffsetFromRenderer);
#if PLATFORM(IOS_FAMILY)
        // Account for the fact that RenderLayerBacking::updateGeometry() bakes scrollOffset into offsetFromRenderer on iOS,
        // but the repaint rect is computed without taking the scroll position into account (see shouldApplyClipAndScrollPositionForRepaint()).
        layerDirtyRect.moveBy(-m_owningLayer.scrollPosition());
#endif
        m_scrollingContentsLayer->setNeedsDisplayInRect(layerDirtyRect, shouldClip);
    }
}

void RenderLayerBacking::paintIntoLayer(const GraphicsLayer* graphicsLayer, GraphicsContext& context,
    const IntRect& paintDirtyRect, // In the coords of rootLayer.
    OptionSet<PaintBehavior> paintBehavior, GraphicsLayerPaintingPhase paintingPhase)
{
    if ((paintsIntoWindow() || paintsIntoCompositedAncestor()) && paintingPhase != GraphicsLayerPaintChildClippingMask) {
#if !PLATFORM(IOS_FAMILY) && !OS(WINDOWS)
        // FIXME: Looks like the CALayer tree is out of sync with the GraphicsLayer heirarchy
        // when pages are restored from the PageCache.
        // <rdar://problem/8712587> ASSERT: When Going Back to Page with Plugins in PageCache
        ASSERT_NOT_REACHED();
#endif
        return;
    }

    OptionSet<RenderLayer::PaintLayerFlag> paintFlags;
    if (paintingPhase & GraphicsLayerPaintBackground)
        paintFlags.add(RenderLayer::PaintLayerPaintingCompositingBackgroundPhase);
    if (paintingPhase & GraphicsLayerPaintForeground)
        paintFlags.add(RenderLayer::PaintLayerPaintingCompositingForegroundPhase);
    if (paintingPhase & GraphicsLayerPaintMask)
        paintFlags.add(RenderLayer::PaintLayerPaintingCompositingMaskPhase);
    if (paintingPhase & GraphicsLayerPaintClipPath)
        paintFlags.add(RenderLayer::PaintLayerPaintingCompositingClipPathPhase);
    if (paintingPhase & GraphicsLayerPaintChildClippingMask)
        paintFlags.add(RenderLayer::PaintLayerPaintingChildClippingMaskPhase);
    if (paintingPhase & GraphicsLayerPaintOverflowContents)
        paintFlags.add(RenderLayer::PaintLayerPaintingOverflowContents);
    if (paintingPhase & GraphicsLayerPaintCompositedScroll)
        paintFlags.add(RenderLayer::PaintLayerPaintingCompositingScrollingPhase);

    if (graphicsLayer == m_backgroundLayer.get() && m_backgroundLayerPaintsFixedRootBackground)
        paintFlags.add({ RenderLayer::PaintLayerPaintingRootBackgroundOnly, RenderLayer::PaintLayerPaintingCompositingForegroundPhase }); // Need PaintLayerPaintingCompositingForegroundPhase to walk child layers.
    else if (compositor().fixedRootBackgroundLayer())
        paintFlags.add(RenderLayer::PaintLayerPaintingSkipRootBackground);

#ifndef NDEBUG
    RenderElement::SetLayoutNeededForbiddenScope forbidSetNeedsLayout(&renderer());
#endif

    FrameView::PaintingState paintingState;
    if (m_owningLayer.isRenderViewLayer())
        renderer().view().frameView().willPaintContents(context, paintDirtyRect, paintingState);

    // FIXME: GraphicsLayers need a way to split for RenderFragmentContainers.
    RenderLayer::LayerPaintingInfo paintingInfo(&m_owningLayer, paintDirtyRect, paintBehavior, -m_subpixelOffsetFromRenderer);
    m_owningLayer.paintLayerContents(context, paintingInfo, paintFlags);

    if (m_owningLayer.containsDirtyOverlayScrollbars())
        m_owningLayer.paintLayerContents(context, paintingInfo, paintFlags | RenderLayer::PaintLayerPaintingOverlayScrollbars);

    if (m_owningLayer.isRenderViewLayer())
        renderer().view().frameView().didPaintContents(context, paintDirtyRect, paintingState);

    compositor().didPaintBacking(this);

    ASSERT(!m_owningLayer.m_usedTransparency);
}

// Up-call from compositing layer drawing callback.
void RenderLayerBacking::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& context, GraphicsLayerPaintingPhase paintingPhase, const FloatRect& clip, GraphicsLayerPaintBehavior layerPaintBehavior)
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
        || graphicsLayer == m_childClippingMaskLayer.get()
        || graphicsLayer == m_scrollingContentsLayer.get()) {
        InspectorInstrumentation::willPaint(renderer());

        if (!(paintingPhase & GraphicsLayerPaintOverflowContents))
            dirtyRect.intersect(enclosingIntRect(compositedBoundsIncludingMargin()));

        // We have to use the same root as for hit testing, because both methods can compute and cache clipRects.
        OptionSet<PaintBehavior> behavior = PaintBehavior::Normal;
        if (layerPaintBehavior == GraphicsLayerPaintSnapshotting)
            behavior.add(PaintBehavior::Snapshotting);
        
        if (layerPaintBehavior == GraphicsLayerPaintFirstTilePaint)
            behavior.add(PaintBehavior::TileFirstPaint);

        paintIntoLayer(graphicsLayer, context, dirtyRect, behavior, paintingPhase);

        InspectorInstrumentation::didPaint(renderer(), dirtyRect);
    } else if (graphicsLayer == layerForHorizontalScrollbar()) {
        paintScrollbar(m_owningLayer.horizontalScrollbar(), context, dirtyRect);
    } else if (graphicsLayer == layerForVerticalScrollbar()) {
        paintScrollbar(m_owningLayer.verticalScrollbar(), context, dirtyRect);
    } else if (graphicsLayer == layerForScrollCorner()) {
        const LayoutRect& scrollCornerAndResizer = m_owningLayer.scrollCornerAndResizerRect();
        context.save();
        context.translate(-scrollCornerAndResizer.location());
        LayoutRect transformedClip = LayoutRect(clip);
        transformedClip.moveBy(scrollCornerAndResizer.location());
        m_owningLayer.paintScrollCorner(context, IntPoint(), snappedIntRect(transformedClip));
        m_owningLayer.paintResizer(context, IntPoint(), transformedClip);
        context.restore();
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
        transform = m_owningLayer.currentTransform(RenderStyle::ExcludeTransformOrigin);
        return true;
    }
    return false;
}

bool RenderLayerBacking::isTrackingRepaints() const
{
    return static_cast<GraphicsLayerClient&>(compositor()).isTrackingRepaints();
}

bool RenderLayerBacking::shouldSkipLayerInDump(const GraphicsLayer* layer, LayerTreeAsTextBehavior behavior) const
{
    if (behavior & LayerTreeAsTextDebug)
        return false;

    // Skip the root tile cache's flattening layer.
    return m_isMainFrameRenderViewLayer && layer && layer == m_childContainmentLayer.get();
}

bool RenderLayerBacking::shouldDumpPropertyForLayer(const GraphicsLayer* layer, const char* propertyName) const
{
    // For backwards compatibility with WebKit1 and other platforms,
    // skip some properties on the root tile cache.
    if (m_isMainFrameRenderViewLayer && layer == m_graphicsLayer.get()) {
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

bool RenderLayerBacking::startAnimation(double timeOffset, const Animation* anim, const KeyframeList& keyframes)
{
    bool hasOpacity = keyframes.containsProperty(CSSPropertyOpacity);
    bool hasTransform = renderer().isBox() && keyframes.containsProperty(CSSPropertyTransform);
    bool hasFilter = keyframes.containsProperty(CSSPropertyFilter);

    bool hasBackdropFilter = false;
#if ENABLE(FILTERS_LEVEL_2)
    hasBackdropFilter = keyframes.containsProperty(CSSPropertyWebkitBackdropFilter);
#endif

    if (!hasOpacity && !hasTransform && !hasFilter && !hasBackdropFilter)
        return false;

    KeyframeValueList transformVector(AnimatedPropertyTransform);
    KeyframeValueList opacityVector(AnimatedPropertyOpacity);
    KeyframeValueList filterVector(AnimatedPropertyFilter);
#if ENABLE(FILTERS_LEVEL_2)
    KeyframeValueList backdropFilterVector(AnimatedPropertyWebkitBackdropFilter);
#endif

    size_t numKeyframes = keyframes.size();
    for (size_t i = 0; i < numKeyframes; ++i) {
        const KeyframeValue& currentKeyframe = keyframes[i];
        const RenderStyle* keyframeStyle = currentKeyframe.style();
        double key = currentKeyframe.key();

        if (!keyframeStyle)
            continue;
            
        auto* tf = currentKeyframe.timingFunction();
        
        bool isFirstOrLastKeyframe = key == 0 || key == 1;
        if ((hasTransform && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyTransform))
            transformVector.insert(std::make_unique<TransformAnimationValue>(key, keyframeStyle->transform(), tf));

        if ((hasOpacity && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyOpacity))
            opacityVector.insert(std::make_unique<FloatAnimationValue>(key, keyframeStyle->opacity(), tf));

        if ((hasFilter && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyFilter))
            filterVector.insert(std::make_unique<FilterAnimationValue>(key, keyframeStyle->filter(), tf));

#if ENABLE(FILTERS_LEVEL_2)
        if ((hasBackdropFilter && isFirstOrLastKeyframe) || currentKeyframe.containsProperty(CSSPropertyWebkitBackdropFilter))
            backdropFilterVector.insert(std::make_unique<FilterAnimationValue>(key, keyframeStyle->backdropFilter(), tf));
#endif
    }

    if (!renderer().settings().acceleratedCompositedAnimationsEnabled())
        return false;

    bool didAnimate = false;

    if (hasTransform && m_graphicsLayer->addAnimation(transformVector, snappedIntRect(renderBox()->borderBoxRect()).size(), anim, keyframes.animationName(), timeOffset))
        didAnimate = true;

    if (hasOpacity && m_graphicsLayer->addAnimation(opacityVector, IntSize(), anim, keyframes.animationName(), timeOffset))
        didAnimate = true;

    if (hasFilter && m_graphicsLayer->addAnimation(filterVector, IntSize(), anim, keyframes.animationName(), timeOffset))
        didAnimate = true;

#if ENABLE(FILTERS_LEVEL_2)
    if (hasBackdropFilter && m_graphicsLayer->addAnimation(backdropFilterVector, IntSize(), anim, keyframes.animationName(), timeOffset))
        didAnimate = true;
#endif

    if (didAnimate)
        m_owningLayer.setNeedsPostLayoutCompositingUpdate();

    return didAnimate;
}

void RenderLayerBacking::animationPaused(double timeOffset, const String& animationName)
{
    m_graphicsLayer->pauseAnimation(animationName, timeOffset);
}

void RenderLayerBacking::animationSeeked(double timeOffset, const String& animationName)
{
    m_graphicsLayer->seekAnimation(animationName, timeOffset);
}

void RenderLayerBacking::animationFinished(const String& animationName)
{
    m_graphicsLayer->removeAnimation(animationName);
    m_owningLayer.setNeedsPostLayoutCompositingUpdate();
}

bool RenderLayerBacking::startTransition(double timeOffset, CSSPropertyID property, const RenderStyle* fromStyle, const RenderStyle* toStyle)
{
    bool didAnimate = false;

    ASSERT(property != CSSPropertyInvalid);

    if (property == CSSPropertyOpacity) {
        const Animation* opacityAnim = toStyle->transitionForProperty(CSSPropertyOpacity);
        if (opacityAnim && !opacityAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList opacityVector(AnimatedPropertyOpacity);
            opacityVector.insert(std::make_unique<FloatAnimationValue>(0, compositingOpacity(fromStyle->opacity())));
            opacityVector.insert(std::make_unique<FloatAnimationValue>(1, compositingOpacity(toStyle->opacity())));
            // The boxSize param is only used for transform animations (which can only run on RenderBoxes), so we pass an empty size here.
            if (m_graphicsLayer->addAnimation(opacityVector, FloatSize(), opacityAnim, GraphicsLayer::animationNameForTransition(AnimatedPropertyOpacity), timeOffset)) {
                // To ensure that the correct opacity is visible when the animation ends, also set the final opacity.
                updateOpacity(*toStyle);
                didAnimate = true;
            }
        }
    }

    if (property == CSSPropertyTransform && m_owningLayer.hasTransform()) {
        const Animation* transformAnim = toStyle->transitionForProperty(CSSPropertyTransform);
        if (transformAnim && !transformAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList transformVector(AnimatedPropertyTransform);
            transformVector.insert(std::make_unique<TransformAnimationValue>(0, fromStyle->transform()));
            transformVector.insert(std::make_unique<TransformAnimationValue>(1, toStyle->transform()));
            if (m_graphicsLayer->addAnimation(transformVector, snappedIntRect(renderBox()->borderBoxRect()).size(), transformAnim, GraphicsLayer::animationNameForTransition(AnimatedPropertyTransform), timeOffset)) {
                // To ensure that the correct transform is visible when the animation ends, also set the final transform.
                updateTransform(*toStyle);
                didAnimate = true;
            }
        }
    }

    if (property == CSSPropertyFilter && m_owningLayer.hasFilter()) {
        const Animation* filterAnim = toStyle->transitionForProperty(CSSPropertyFilter);
        if (filterAnim && !filterAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList filterVector(AnimatedPropertyFilter);
            filterVector.insert(std::make_unique<FilterAnimationValue>(0, fromStyle->filter()));
            filterVector.insert(std::make_unique<FilterAnimationValue>(1, toStyle->filter()));
            if (m_graphicsLayer->addAnimation(filterVector, FloatSize(), filterAnim, GraphicsLayer::animationNameForTransition(AnimatedPropertyFilter), timeOffset)) {
                // To ensure that the correct filter is visible when the animation ends, also set the final filter.
                updateFilters(*toStyle);
                didAnimate = true;
            }
        }
    }

#if ENABLE(FILTERS_LEVEL_2)
    if (property == CSSPropertyWebkitBackdropFilter && m_owningLayer.hasBackdropFilter()) {
        const Animation* backdropFilterAnim = toStyle->transitionForProperty(CSSPropertyWebkitBackdropFilter);
        if (backdropFilterAnim && !backdropFilterAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList backdropFilterVector(AnimatedPropertyWebkitBackdropFilter);
            backdropFilterVector.insert(std::make_unique<FilterAnimationValue>(0, fromStyle->backdropFilter()));
            backdropFilterVector.insert(std::make_unique<FilterAnimationValue>(1, toStyle->backdropFilter()));
            if (m_graphicsLayer->addAnimation(backdropFilterVector, FloatSize(), backdropFilterAnim, GraphicsLayer::animationNameForTransition(AnimatedPropertyWebkitBackdropFilter), timeOffset)) {
                // To ensure that the correct backdrop filter is visible when the animation ends, also set the final backdrop filter.
                updateBackdropFilters(*toStyle);
                didAnimate = true;
            }
        }
    }
#endif

    if (didAnimate)
        m_owningLayer.setNeedsPostLayoutCompositingUpdate();

    return didAnimate;
}

void RenderLayerBacking::transitionPaused(double timeOffset, CSSPropertyID property)
{
    AnimatedPropertyID animatedProperty = cssToGraphicsLayerProperty(property);
    if (animatedProperty != AnimatedPropertyInvalid)
        m_graphicsLayer->pauseAnimation(GraphicsLayer::animationNameForTransition(animatedProperty), timeOffset);
}

void RenderLayerBacking::transitionFinished(CSSPropertyID property)
{
    AnimatedPropertyID animatedProperty = cssToGraphicsLayerProperty(property);
    if (animatedProperty != AnimatedPropertyInvalid) {
        m_graphicsLayer->removeAnimation(GraphicsLayer::animationNameForTransition(animatedProperty));
        m_owningLayer.setNeedsPostLayoutCompositingUpdate();
    }
}

void RenderLayerBacking::notifyAnimationStarted(const GraphicsLayer*, const String&, MonotonicTime time)
{
    renderer().animation().notifyAnimationStarted(renderer(), time);
}

void RenderLayerBacking::notifyFlushRequired(const GraphicsLayer* layer)
{
    if (renderer().renderTreeBeingDestroyed())
        return;
    compositor().scheduleLayerFlush(layer->canThrottleLayerFlush());
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
    
    // m_ancestorClippingLayer, m_contentsContainmentLayer and m_childContainmentLayer are just used for masking or containment, so have no backing.
    backingMemory = m_graphicsLayer->backingStoreMemoryEstimate();
    if (m_foregroundLayer)
        backingMemory += m_foregroundLayer->backingStoreMemoryEstimate();
    if (m_backgroundLayer)
        backingMemory += m_backgroundLayer->backingStoreMemoryEstimate();
    if (m_maskLayer)
        backingMemory += m_maskLayer->backingStoreMemoryEstimate();
    if (m_childClippingMaskLayer)
        backingMemory += m_childClippingMaskLayer->backingStoreMemoryEstimate();

    if (m_scrollingContentsLayer)
        backingMemory += m_scrollingContentsLayer->backingStoreMemoryEstimate();

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
    if (auto nodeID = backing.scrollingNodeIDForRole(ScrollCoordinationRole::FrameHosting))
        ts << " frame hosting node " << nodeID;
    return ts;
}

} // namespace WebCore
