/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#if ENABLE(PDFKIT_PAINTED_SELECTIONS)
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
        // FIXME: This instantiates PDFPages needlessly, just to determine if they exist.
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
    RefPtr pageBackgroundsContainerLayer = m_pageBackgroundsContainerLayer;
    if (!pageBackgroundsContainerLayer) {
        pageBackgroundsContainerLayer = createGraphicsLayer("Page backgrounds"_s, GraphicsLayer::Type::Normal);
        m_pageBackgroundsContainerLayer = pageBackgroundsContainerLayer.copyRef();
        pageBackgroundsContainerLayer->setAnchorPoint({ });
        scrolledContentsLayer.addChild(pageBackgroundsContainerLayer.releaseNonNull());
    }

    RefPtr contentsLayer = m_contentsLayer;
    if (!contentsLayer) {
        contentsLayer = createGraphicsLayer("PDF contents"_s, m_plugin->isFullMainFramePlugin() ? GraphicsLayer::Type::PageTiledBacking : GraphicsLayer::Type::TiledBacking);
        m_contentsLayer = contentsLayer.copyRef();
        contentsLayer->setAnchorPoint({ });
        contentsLayer->setDrawsContent(true);
        contentsLayer->setAcceleratesDrawing(!shouldUseInProcessBackingStore());
        scrolledContentsLayer.addChild(*contentsLayer);

        // This is the call that enables async rendering.
        asyncRenderer()->startTrackingLayer(*contentsLayer);
    }

#if ENABLE(PDFKIT_PAINTED_SELECTIONS)
    RefPtr selectionLayer = m_selectionLayer;
    if (!selectionLayer) {
        selectionLayer = createGraphicsLayer("PDF selections"_s, GraphicsLayer::Type::TiledBacking);
        m_selectionLayer = selectionLayer.copyRef();
        selectionLayer->setAnchorPoint({ });
        selectionLayer->setDrawsContent(true);
        selectionLayer->setAcceleratesDrawing(true);
        selectionLayer->setBlendMode(BlendMode::Multiply);

        // m_selectionLayer will be parented on-demand in `setSelectionLayerEnabled`.
    }
#endif
}

void PDFScrollingPresentationController::updateLayersOnLayoutChange(FloatSize documentSize, FloatSize centeringOffset, double scaleFactor)
{
    Ref contentsLayer = *m_contentsLayer;
    contentsLayer->setSize(documentSize);
    contentsLayer->setNeedsDisplay();

    TransformationMatrix transform;
    transform.scale(scaleFactor);
    transform.translate(centeringOffset.width(), centeringOffset.height());

    contentsLayer->setTransform(transform);
    protectedPageBackgroundsContainerLayer()->setTransform(transform);

#if ENABLE(PDFKIT_PAINTED_SELECTIONS)
    Ref selectionLayer = *m_selectionLayer;
    selectionLayer->setSize(documentSize);
    selectionLayer->setNeedsDisplay();
    selectionLayer->setTransform(transform);
#endif

    updatePageBackgroundLayers();
}

void PDFScrollingPresentationController::updatePageBackgroundLayers()
{
    auto& documentLayout = m_plugin->documentLayout();

    Ref pageBackgroundsContainerLayer = *m_pageBackgroundsContainerLayer;
    Vector<Ref<GraphicsLayer>> pageContainerLayers = pageBackgroundsContainerLayer->children();

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

    pageBackgroundsContainerLayer->setChildren(WTFMove(pageContainerLayers));
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

void PDFScrollingPresentationController::updateIsInWindow(bool isInWindow)
{
    protectedContentsLayer()->setIsInWindow(isInWindow);

#if ENABLE(PDFKIT_PAINTED_SELECTIONS)
    protectedSelectionLayer()->setIsInWindow(isInWindow);
#endif

    for (auto& pageLayer : protectedPageBackgroundsContainerLayer()->children()) {
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

    if (RefPtr pageBackgroundsContainerLayer = m_pageBackgroundsContainerLayer)
        propagateSettingsToLayer(*pageBackgroundsContainerLayer);

    if (RefPtr contentsLayer = m_contentsLayer)
        propagateSettingsToLayer(*contentsLayer);

#if ENABLE(PDFKIT_PAINTED_SELECTIONS)
    if (RefPtr selectionLayer = m_selectionLayer)
        propagateSettingsToLayer(*selectionLayer);
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
    RefPtr contentsLayer = m_contentsLayer;
    if (!contentsLayer)
        return;
    if (CheckedPtr tiledBacking = contentsLayer->tiledBacking())
        tiledBacking->setScrollability(scrollability);

#if ENABLE(PDFKIT_PAINTED_SELECTIONS)
    if (CheckedPtr tiledBacking = protectedSelectionLayer()->tiledBacking())
        tiledBacking->setScrollability(scrollability);
#endif
}

auto PDFScrollingPresentationController::layerCoveragesForRepaintPageCoverage(RepaintRequirements repaintRequirements, const PDFPageCoverage& pageCoverage) -> Vector<LayerCoverage>
{
    Vector<LayerCoverage> result;
    FloatRect contentsRect;
    for (auto& perPage : pageCoverage)
        contentsRect.unite(m_plugin->convertUp(UnifiedPDFPlugin::CoordinateSpace::PDFPage, UnifiedPDFPlugin::CoordinateSpace::Contents, perPage.rectInPageLayoutCoordinates, perPage.pageIndex));

#if ENABLE(PDFKIT_PAINTED_SELECTIONS)
    if (repaintRequirements.contains(RepaintRequirement::Selection))
        result.append({ *m_selectionLayer, contentsRect, RepaintRequirements { RepaintRequirement::Selection } });
#endif

    if (repaintRequirements.contains(RepaintRequirement::PDFContent))
        result.append({ *m_contentsLayer, contentsRect, RepaintRequirements { RepaintRequirement::PDFContent } });
    return result;
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

std::optional<PDFDocumentLayout::PageIndex> PDFScrollingPresentationController::pageIndexForCurrentView(AnchorPoint anchorPoint) const
{
    return m_plugin->documentLayout().pageIndexAndPagePointForDocumentYOffset(anchorPointInDocumentSpace(anchorPoint).y()).first;
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
        return scaleForPagePreviews();

    return { };
}

bool PDFScrollingPresentationController::layerNeedsPlatformContext(const GraphicsLayer* layer) const
{
    return shouldUseInProcessBackingStore() && (layer == m_contentsLayer || pageIndexForPageBackgroundLayer(layer));
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
bool PDFScrollingPresentationController::layerAllowsDynamicContentScaling(const GraphicsLayer*) const
{
    // Provide DCS structures explicitly.
    return false;
}
#endif

void PDFScrollingPresentationController::tiledBackingUsageChanged(const GraphicsLayer* layer, bool usingTiledBacking)
{
    if (usingTiledBacking)
        layer->checkedTiledBacking()->setIsInWindow(m_plugin->isInWindow());
}

void PDFScrollingPresentationController::paintContents(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, OptionSet<GraphicsLayerPaintBehavior>)
{
    if (layer == m_contentsLayer.get()) {
        RefPtr asyncRenderer = asyncRendererIfExists();
        m_plugin->paintPDFContent(layer, context, clipRect, { }, asyncRenderer.get());
        return;
    }

#if ENABLE(PDFKIT_PAINTED_SELECTIONS)
    if (layer == m_selectionLayer.get()) {
        paintPDFSelection(layer, context, clipRect, { });
        return;
    }
#endif

    if (auto backgroundLayerPageIndex = pageIndexForPageBackgroundLayer(layer)) {
        paintBackgroundLayerForPage(layer, context, clipRect, *backgroundLayerPageIndex);
        return;
    }
}

void PDFScrollingPresentationController::paintPDFSelection(const GraphicsLayer* layer, GraphicsContext& context, const FloatRect& clipRect, std::optional<PDFLayoutRow> row)
{
    m_plugin->paintPDFSelection(layer, context, clipRect, row);
}

void PDFScrollingPresentationController::setSelectionLayerEnabled(bool enabled)
{
#if ENABLE(PDFKIT_PAINTED_SELECTIONS)
    Ref selectionLayer = *m_selectionLayer;
    bool wasEnabled = !!selectionLayer->parent();
    if (wasEnabled == enabled)
        return;
    if (!enabled)
        selectionLayer->removeFromParent();
    else
        m_contentsLayer->protectedParent()->addChild(WTFMove(selectionLayer));
#endif
}

std::optional<PlatformLayerIdentifier> PDFScrollingPresentationController::contentsLayerIdentifier() const
{
    RefPtr contentsLayer = m_contentsLayer;
    if (!contentsLayer)
        return std::nullopt;

    return contentsLayer->primaryLayerID();
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
