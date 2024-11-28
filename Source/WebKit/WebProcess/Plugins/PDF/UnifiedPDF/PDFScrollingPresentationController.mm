/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PDFScrollingPresentationController.h"

#if ENABLE(UNIFIED_PDF)

#include "AsyncPDFRenderer.h"
#include "Logging.h"
#include "UnifiedPDFPlugin.h"
#include "WebKeyboardEvent.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/KeyboardScrollingAnimator.h>
#include <WebCore/ScrollAnimator.h>
#include <WebCore/TiledBacking.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PDFScrollingPresentationController);

PDFScrollingPresentationController::PDFScrollingPresentationController(UnifiedPDFPlugin& plugin)
    : PDFPresentationController(plugin)
{

}

void PDFScrollingPresentationController::teardown()
{
    PDFPresentationController::teardown();

    GraphicsLayer::unparentAndClear(m_contentsLayer);
    GraphicsLayer::unparentAndClear(m_pageBackgroundsContainerLayer);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    GraphicsLayer::unparentAndClear(m_selectionLayer);
#endif
}

bool PDFScrollingPresentationController::supportsDisplayMode(PDFDocumentLayout::DisplayMode mode) const
{
    return PDFDocumentLayout::isScrollingDisplayMode(mode);
}

#pragma mark -

bool PDFScrollingPresentationController::handleKeyboardEvent(const WebKeyboardEvent& event)
{
#if PLATFORM(MAC)
    if (handleKeyboardCommand(event))
        return true;
#endif

    return false;
}

#if PLATFORM(MAC)
CheckedPtr<WebCore::KeyboardScrollingAnimator> PDFScrollingPresentationController::checkedKeyboardScrollingAnimator() const
{
    return m_plugin->scrollAnimator().keyboardScrollingAnimator();
}

bool PDFScrollingPresentationController::handleKeyboardCommand(const WebKeyboardEvent& event)
{
    auto& commands = event.commands();
    if (commands.size() != 1)
        return false;

    auto commandName = commands[0].commandName;
    if (commandName == "scrollToBeginningOfDocument:"_s)
        return checkedKeyboardScrollingAnimator()->beginKeyboardScrollGesture(ScrollDirection::ScrollUp, ScrollGranularity::Document, false);

    if (commandName == "scrollToEndOfDocument:"_s)
        return checkedKeyboardScrollingAnimator()->beginKeyboardScrollGesture(ScrollDirection::ScrollDown, ScrollGranularity::Document, false);

    return false;
}
#endif

#pragma mark -

PDFPageCoverage PDFScrollingPresentationController::pageCoverageForContentsRect(const FloatRect& contentsRect, std::optional<PDFLayoutRow>) const
{
    if (m_plugin->visibleOrDocumentSizeIsEmpty())
        return { };

    auto rectInPDFLayoutCoordinates = m_plugin->convertDown(UnifiedPDFPlugin::CoordinateSpace::Contents, UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, contentsRect);

    auto& documentLayout = m_plugin->documentLayout();
    auto pageCoverage = PDFPageCoverage { };
    for (PDFDocumentLayout::PageIndex i = 0; i < documentLayout.pageCount(); ++i) {
        auto page = documentLayout.pageAtIndex(i);
        if (!page)
            continue;

        auto pageBounds = layoutBoundsForPageAtIndex(i);
        if (!pageBounds.intersects(rectInPDFLayoutCoordinates))
            continue;

        pageCoverage.append(PerPageInfo { i, pageBounds, rectInPDFLayoutCoordinates });
    }

    return pageCoverage;
}

PDFPageCoverageAndScales PDFScrollingPresentationController::pageCoverageAndScalesForContentsRect(const FloatRect& clipRect, std::optional<PDFLayoutRow> row, float tilingScaleFactor) const
{
    if (m_plugin->visibleOrDocumentSizeIsEmpty())
        return { { }, { }, 1, 1, 1 };

    auto pageCoverageAndScales = PDFPageCoverageAndScales { pageCoverageForContentsRect(clipRect, row) };

    pageCoverageAndScales.deviceScaleFactor = m_plugin->deviceScaleFactor();
    pageCoverageAndScales.pdfDocumentScale = m_plugin->documentLayout().scale();
    pageCoverageAndScales.tilingScaleFactor = tilingScaleFactor;

    return pageCoverageAndScales;
}

void PDFScrollingPresentationController::setupLayers(GraphicsLayer& scrolledContentsLayer)
{
    if (!m_pageBackgroundsContainerLayer) {
        m_pageBackgroundsContainerLayer = createGraphicsLayer("Page backgrounds"_s, GraphicsLayer::Type::Normal);
        m_pageBackgroundsContainerLayer->setAnchorPoint({ });
        scrolledContentsLayer.addChild(*m_pageBackgroundsContainerLayer);
    }

    if (!m_contentsLayer) {
        m_contentsLayer = createGraphicsLayer("PDF contents"_s, m_plugin->isFullMainFramePlugin() ? GraphicsLayer::Type::PageTiledBacking : GraphicsLayer::Type::TiledBacking);
        m_contentsLayer->setAnchorPoint({ });
        m_contentsLayer->setDrawsContent(true);
        m_contentsLayer->setAcceleratesDrawing(m_plugin->canPaintSelectionIntoOwnedLayer());
        scrolledContentsLayer.addChild(*m_contentsLayer);

        // This is the call that enables async rendering.
        asyncRenderer()->startTrackingLayer(*m_contentsLayer);
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (!m_selectionLayer) {
        m_selectionLayer = createGraphicsLayer("PDF selections"_s, GraphicsLayer::Type::TiledBacking);
        m_selectionLayer->setAnchorPoint({ });
        m_selectionLayer->setDrawsContent(true);
        m_selectionLayer->setAcceleratesDrawing(true);
        m_selectionLayer->setBlendMode(BlendMode::Multiply);
        scrolledContentsLayer.addChild(*m_selectionLayer);
    }
#endif
}

void PDFScrollingPresentationController::updateLayersOnLayoutChange(FloatSize documentSize, FloatSize centeringOffset, double scaleFactor)
{
    m_contentsLayer->setSize(documentSize);
    m_contentsLayer->setNeedsDisplay();

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    m_selectionLayer->setSize(documentSize);
    m_selectionLayer->setNeedsDisplay();
#endif

    TransformationMatrix transform;
    transform.scale(scaleFactor);
    transform.translate(centeringOffset.width(), centeringOffset.height());

    m_contentsLayer->setTransform(transform);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    m_selectionLayer->setTransform(transform);
#endif
    m_pageBackgroundsContainerLayer->setTransform(transform);

    updatePageBackgroundLayers();
}

void PDFScrollingPresentationController::updatePageBackgroundLayers()
{
    auto& documentLayout = m_plugin->documentLayout();

    Vector<Ref<GraphicsLayer>> pageContainerLayers = m_pageBackgroundsContainerLayer->children();

    // pageContentsLayers are always the size of `layoutBoundsForPageAtIndex`; we generate a page preview
    // buffer of the same size. On zooming, this layer just gets scaled, to avoid repainting.

    for (PDFDocumentLayout::PageIndex i = 0; i < documentLayout.pageCount(); ++i) {
        auto pageBoundsRect = layoutBoundsForPageAtIndex(i);
        auto destinationRect = pageBoundsRect;
        destinationRect.scale(documentLayout.scale());

        auto pageContainerLayer = [&](PDFDocumentLayout::PageIndex pageIndex) {
            if (pageIndex < pageContainerLayers.size())
                return pageContainerLayers[pageIndex];

            RefPtr pageContainerLayer = makePageContainerLayer(pageIndex);

            // Sure would be nice if we could just stuff data onto a GraphicsLayer.
            RefPtr pageBackgroundLayer = pageBackgroundLayerForPageContainerLayer(*pageContainerLayer);
            m_pageBackgroundLayers.add(pageBackgroundLayer, pageIndex);

            auto containerLayer = pageContainerLayer.releaseNonNull();
            pageContainerLayers.append(WTFMove(containerLayer));

            return pageContainerLayers[pageIndex];
        }(i);

        pageContainerLayer->setPosition(destinationRect.location());
        pageContainerLayer->setSize(destinationRect.size());

        auto pageBackgroundLayer = pageContainerLayer->children()[0];
        pageBackgroundLayer->setSize(pageBoundsRect.size());

        TransformationMatrix documentScaleTransform;
        documentScaleTransform.scale(documentLayout.scale());
        pageBackgroundLayer->setTransform(documentScaleTransform);
    }

    m_pageBackgroundsContainerLayer->setChildren(WTFMove(pageContainerLayers));
}

GraphicsLayer* PDFScrollingPresentationController::backgroundLayerForPage(PDFDocumentLayout::PageIndex pageIndex) const
{
    if (!m_pageBackgroundsContainerLayer)
        return nullptr;

    auto pageContainerLayers = m_pageBackgroundsContainerLayer->children();
    if (pageContainerLayers.size() <= pageIndex)
        return nullptr;

    Ref pageContainerLayer = pageContainerLayers[pageIndex];
    if (!pageContainerLayer->children().size())
        return nullptr;

    return pageContainerLayer->children()[0].ptr();
}

void PDFScrollingPresentationController::didGeneratePreviewForPage(PDFDocumentLayout::PageIndex pageIndex)
{
    if (RefPtr layer = backgroundLayerForPage(pageIndex))
        layer->setNeedsDisplay();
}

void PDFScrollingPresentationController::repaintForIncrementalLoad()
{
    auto& documentLayout = m_plugin->documentLayout();
    auto coverageRect = FloatRect { { }, documentLayout.contentsSize() };

    if (auto* tiledBacking = m_contentsLayer->tiledBacking()) {
        coverageRect = tiledBacking->coverageRect();
        coverageRect = m_plugin->convertDown(UnifiedPDFPlugin::CoordinateSpace::Contents, UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, coverageRect);
    }

    setNeedsRepaintInDocumentRect(RepaintRequirement::PDFContent, coverageRect, { });
}

void PDFScrollingPresentationController::updateIsInWindow(bool isInWindow)
{
    m_contentsLayer->setIsInWindow(isInWindow);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (m_selectionLayer)
        m_selectionLayer->setIsInWindow(isInWindow);
#endif

    for (auto& pageLayer : m_pageBackgroundsContainerLayer->children()) {
        if (pageLayer->children().size()) {
            Ref pageContentsLayer = pageLayer->children()[0];
            pageContentsLayer->setIsInWindow(isInWindow);
        }
    }
}

void PDFScrollingPresentationController::updateDebugBorders(bool showDebugBorders, bool showRepaintCounters)
{
    auto propagateSettingsToLayer = [&] (GraphicsLayer& layer) {
        layer.setShowDebugBorder(showDebugBorders);
        layer.setShowRepaintCounter(showRepaintCounters);
    };

    if (m_pageBackgroundsContainerLayer)
        propagateSettingsToLayer(*m_pageBackgroundsContainerLayer);

    if (m_contentsLayer)
        propagateSettingsToLayer(*m_contentsLayer);

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (m_selectionLayer)
        propagateSettingsToLayer(*m_selectionLayer);
#endif

    if (m_pageBackgroundsContainerLayer) {
        for (auto& pageLayer : m_pageBackgroundsContainerLayer->children()) {
            propagateSettingsToLayer(pageLayer);
            if (pageLayer->children().size())
                propagateSettingsToLayer(pageLayer->children()[0]);
        }
    }

    if (RefPtr asyncRenderer = asyncRendererIfExists())
        asyncRenderer->setShowDebugBorders(showDebugBorders);
}

void PDFScrollingPresentationController::updateForCurrentScrollability(OptionSet<TiledBackingScrollability> scrollability)
{
    if (auto* tiledBacking = m_contentsLayer->tiledBacking())
        tiledBacking->setScrollability(scrollability);
}

void PDFScrollingPresentationController::setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement> repaintRequirements, const FloatRect& rectInDocumentCoordinates, std::optional<PDFLayoutRow> layoutRow)
{
    if (!repaintRequirements)
        return;

    auto contentsRect = m_plugin->convertUp(UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, UnifiedPDFPlugin::CoordinateSpace::Contents, rectInDocumentCoordinates);
    if (repaintRequirements.contains(RepaintRequirement::PDFContent)) {
        if (RefPtr asyncRenderer = asyncRendererIfExists())
            asyncRenderer->pdfContentChangedInRect(m_contentsLayer.get(), contentsRect, layoutRow);
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (repaintRequirements.contains(RepaintRequirement::Selection) && m_plugin->canPaintSelectionIntoOwnedLayer()) {
        RefPtr { m_selectionLayer }->setNeedsDisplayInRect(contentsRect);
        if (repaintRequirements.hasExactlyOneBitSet())
            return;
    }
#endif

    RefPtr { m_contentsLayer }->setNeedsDisplayInRect(contentsRect);
}

void PDFScrollingPresentationController::paintBackgroundLayerForPage(const GraphicsLayer*, GraphicsContext& context, const FloatRect& clipRect, PDFDocumentLayout::PageIndex pageIndex)
{
    auto destinationRect = layoutBoundsForPageAtIndex(pageIndex);
    destinationRect.setLocation({ });

    if (RefPtr asyncRenderer = asyncRendererIfExists())
        asyncRenderer->paintPagePreview(context, clipRect, destinationRect, pageIndex);
}

std::optional<PDFDocumentLayout::PageIndex> PDFScrollingPresentationController::pageIndexForPageBackgroundLayer(const GraphicsLayer* layer) const
{
    auto it = m_pageBackgroundLayers.find(layer);
    if (it == m_pageBackgroundLayers.end())
        return { };

    return it->value;
}

#pragma mark -

auto PDFScrollingPresentationController::pdfPositionForCurrentView(bool preservePosition) const -> std::optional<VisiblePDFPosition>
{
    if (!preservePosition)
        return { };

    auto& documentLayout = m_plugin->documentLayout();

    if (!documentLayout.hasLaidOutPDFDocument())
        return { };

    auto topLeftInDocumentSpace = m_plugin->convertDown(UnifiedPDFPlugin::CoordinateSpace::Plugin, UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, FloatPoint { });
    auto [pageIndex, pointInPDFPageSpace] = documentLayout.pageIndexAndPagePointForDocumentYOffset(topLeftInDocumentSpace.y());

    LOG_WITH_STREAM(PDF, stream << "PDFScrollingPresentationController::pdfPositionForCurrentView - point " << pointInPDFPageSpace << " in page " << pageIndex);

    return VisiblePDFPosition { pageIndex, pointInPDFPageSpace };
}

void PDFScrollingPresentationController::restorePDFPosition(const VisiblePDFPosition& info)
{
    m_plugin->revealPointInPage(info.pagePoint, info.pageIndex);
}

#pragma mark -

void PDFScrollingPresentationController::notifyFlushRequired(const GraphicsLayer*)
{
    m_plugin->scheduleRenderingUpdate();
}

// This is a GraphicsLayerClient function. The return value is used to compute layer contentsScale, so we don't
// want to use the normalized scale factor.
float PDFScrollingPresentationController::pageScaleFactor() const
{
    return m_plugin->nonNormalizedScaleFactor();
}

float PDFScrollingPresentationController::deviceScaleFactor() const
{
    return m_plugin->deviceScaleFactor();
}

std::optional<float> PDFScrollingPresentationController::customContentsScale(const GraphicsLayer* layer) const
{
    if (pageIndexForPageBackgroundLayer(layer))
        return m_plugin->scaleForPagePreviews();

    return { };
}

bool PDFScrollingPresentationController::layerNeedsPlatformContext(const GraphicsLayer* layer) const
{
    // We need a platform context if the plugin can not paint selections into its own layer,
    // since we would then have to vend a platform context that PDFKit can paint into.
    // However, this constraint only applies for the contents layer. No other layer needs to be WP-backed.
    return layer == m_contentsLayer.get() && !m_plugin->canPaintSelectionIntoOwnedLayer();
}

void PDFScrollingPresentationController::tiledBackingUsageChanged(const GraphicsLayer* layer, bool usingTiledBacking)
{
    if (usingTiledBacking)
        layer->tiledBacking()->setIsInWindow(m_plugin->isInWindow());
}

void PDFScrollingPresentationController::paintContents(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, OptionSet<GraphicsLayerPaintBehavior>)
{

    if (layer == m_contentsLayer.get()) {
        RefPtr asyncRenderer = asyncRendererIfExists();
        m_plugin->paintPDFContent(layer, context, clipRect, { }, UnifiedPDFPlugin::PaintingBehavior::All, asyncRenderer.get());
        return;
    }

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    if (layer == m_selectionLayer.get())
        return paintPDFSelection(layer, context, clipRect, { });
#endif

    if (auto backgroundLayerPageIndex = pageIndexForPageBackgroundLayer(layer)) {
        paintBackgroundLayerForPage(layer, context, clipRect, *backgroundLayerPageIndex);
        return;
    }
}

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
void PDFScrollingPresentationController::paintPDFSelection(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, std::optional<PDFLayoutRow> row)
{
    m_plugin->paintPDFSelection(layer, context, clipRect, row);
}
#endif

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
