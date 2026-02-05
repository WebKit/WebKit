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
#include "PDFPresentationController.h"

#if ENABLE(UNIFIED_PDF)

#include "AsyncPDFRenderer.h"
#include "Logging.h"
#include "PDFDiscretePresentationController.h"
#include "PDFKitSPI.h"
#include "PDFScrollingPresentationController.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Path.h>
#include <WebCore/PathSegment.h>
#include <WebCore/PathSegmentData.h>
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PDFPresentationController);

RefPtr<PDFPresentationController> PDFPresentationController::createForMode(PDFDisplayMode mode, UnifiedPDFPlugin& plugin)
{
    if (PDFDocumentLayout::isScrollingDisplayMode(mode))
        return adoptRef(*new PDFScrollingPresentationController { plugin });

    if (PDFDocumentLayout::isDiscreteDisplayMode(mode))
        return adoptRef(*new PDFDiscretePresentationController { plugin });

    ASSERT_NOT_REACHED();
    return nullptr;
}

PDFPresentationController::PDFPresentationController(UnifiedPDFPlugin& plugin)
    : m_plugin(plugin)
{

}

PDFPresentationController::~PDFPresentationController() = default;

void PDFPresentationController::teardown()
{
    clearAsyncRenderer();
}

Ref<AsyncPDFRenderer> PDFPresentationController::asyncRenderer()
{
    if (m_asyncRenderer)
        return *m_asyncRenderer;

    m_asyncRenderer = AsyncPDFRenderer::create(*this);
    return *m_asyncRenderer;
}

RefPtr<AsyncPDFRenderer> PDFPresentationController::asyncRendererIfExists() const
{
    return m_asyncRenderer;
}

void PDFPresentationController::clearAsyncRenderer()
{
    if (RefPtr asyncRenderer = std::exchange(m_asyncRenderer, nullptr))
        asyncRenderer->teardown();
}

RefPtr<GraphicsLayer> PDFPresentationController::createGraphicsLayer(const String& name, GraphicsLayer::Type layerType)
{
    auto* graphicsLayerFactory = m_plugin->graphicsLayerFactory();
    Ref graphicsLayer = GraphicsLayer::create(graphicsLayerFactory, graphicsLayerClient(), layerType);
    graphicsLayer->setName(name);
    return graphicsLayer;
}

WebCore::Path PDFPresentationController::shadowPathForLayer(const WebCore::GraphicsLayer& layer) const
{
    FloatRect bounds { layer.boundsOrigin(), layer.size() };
    return { { WebCore::PathSegment { WebCore::PathRect { bounds } } } };
}

RefPtr<GraphicsLayer> PDFPresentationController::makePageContainerLayer(PDFDocumentLayout::PageIndex pageIndex)
{
    auto addLayerShadow = [](GraphicsLayer& layer, IntPoint shadowOffset, const Color& shadowColor, int shadowStdDeviation) {
        Vector<Ref<FilterOperation>> filterOperations;
        filterOperations.append(DropShadowFilterOperation::create(shadowColor, shadowOffset, shadowStdDeviation));
        layer.setFilters(FilterOperations { WTF::move(filterOperations) });
    };

    constexpr auto containerShadowOffset = IntPoint { 0, 1 };
    constexpr auto containerShadowColor = SRGBA<uint8_t> { 0, 0, 0, 46 };
    constexpr int containerShadowStdDeviation = 2;

    constexpr auto shadowOffset = IntPoint { 0, 2 };
    constexpr auto shadowColor = SRGBA<uint8_t> { 0, 0, 0, 38 };
    constexpr int shadowStdDeviation = 6;

    RefPtr pageContainerLayer = createGraphicsLayer(makeString("Page container "_s, pageIndex), GraphicsLayer::Type::Normal);
    RefPtr pageBackgroundLayer = createGraphicsLayer(makeString("Page background "_s, pageIndex), GraphicsLayer::Type::Normal);
    // Can only be null if this->page() is null, which we checked above.
    ASSERT(pageContainerLayer);
    ASSERT(pageBackgroundLayer);

    pageContainerLayer->setAnchorPoint({ });

    pageBackgroundLayer->setAnchorPoint({ });
    pageBackgroundLayer->setBackgroundColor(Color::white);
    pageBackgroundLayer->setDrawsContent(true);
    pageBackgroundLayer->setAcceleratesDrawing(!shouldUseInProcessBackingStore());
    pageBackgroundLayer->setShouldUpdateRootRelativeScaleFactor(false);
    pageBackgroundLayer->setAllowsTiling(false);
    pageBackgroundLayer->setNeedsDisplay(); // We only need to paint this layer once when page backgrounds change.

    addLayerShadow(*pageContainerLayer, containerShadowOffset, containerShadowColor, containerShadowStdDeviation);
    // FIXME: <https://webkit.org/b/276981> Need to add a 1px black border with alpha 0.0586.
    addLayerShadow(*pageBackgroundLayer, shadowOffset, shadowColor, shadowStdDeviation);

    pageContainerLayer->addChild(*pageBackgroundLayer);

    return pageContainerLayer;
}

RefPtr<GraphicsLayer> PDFPresentationController::pageBackgroundLayerForPageContainerLayer(GraphicsLayer& pageContainerLayer)
{
    auto& children = pageContainerLayer.children();
    if (children.size()) {
        Ref layer = children[0];
        return WTF::move(layer);
    }

    return nullptr;
}

void PDFPresentationController::releaseMemory()
{
    if (RefPtr asyncRenderer = asyncRendererIfExists())
        asyncRenderer->releaseMemory();
}

RetainPtr<PDFDocument> PDFPresentationController::pluginPDFDocument() const
{
    return m_plugin->pdfDocument();
}

FloatRect PDFPresentationController::layoutBoundsForPageAtIndex(PDFDocumentLayout::PageIndex pageIndex) const
{
    return m_plugin->layoutBoundsForPageAtIndex(pageIndex);
}

bool PDFPresentationController::pluginShouldCachePagePreviews() const
{
    return m_plugin->shouldCachePagePreviews();
}

float PDFPresentationController::scaleForPagePreviews() const
{
    return m_plugin->scaleForPagePreviews();
}

void PDFPresentationController::setNeedsRepaintForPageCoverage(RepaintRequirements repaintRequirements, const PDFPageCoverage& coverage)
{
    // HoverOverlay is currently painted to PDFContent.
    if (repaintRequirements.contains(RepaintRequirement::HoverOverlay)) {
        repaintRequirements.remove(RepaintRequirement::HoverOverlay);
        repaintRequirements.add(RepaintRequirement::PDFContent);
    }

    auto layerCoverages = layerCoveragesForRepaintPageCoverage(repaintRequirements, coverage);
    for (auto& layerCoverage : layerCoverages)
        Ref { layerCoverage.layer }->setNeedsDisplayInRect(layerCoverage.bounds);

    // Unite consecutive PDFContent display rects and send them as render rect to AsyncRenderer.
    if (RefPtr asyncRenderer = asyncRendererIfExists()) {
        RefPtr<GraphicsLayer> layer;
        FloatRect bounds;
        for (auto& layerCoverage : layerCoverages) {
            if (!layerCoverage.repaintRequirements.contains(RepaintRequirement::PDFContent))
                continue;
            if (layerCoverage.layer.ptr() != layer) {
                if (layer && !bounds.isEmpty())
                    asyncRenderer->setNeedsRenderForRect(*layer, bounds);
                layer = layerCoverage.layer.ptr();
                bounds = layerCoverage.bounds;
            } else
                bounds.unite(layerCoverage.bounds);
        }
        if (layer && !bounds.isEmpty())
            asyncRenderer->setNeedsRenderForRect(*layer, bounds);
        asyncRenderer->invalidatePreviewsForPageCoverage(coverage);
    }
}

PDFDocumentLayout::PageIndex PDFPresentationController::nearestPageIndexForDocumentPoint(const FloatPoint& point) const
{
    if (m_plugin->isLocked())
        return 0;
    return m_plugin->documentLayout().nearestPageIndexForDocumentPoint(point, visibleRow());
}

std::optional<PDFDocumentLayout::PageIndex> PDFPresentationController::pageIndexForDocumentPoint(const FloatPoint& point) const
{
    auto& documentLayout = m_plugin->documentLayout();

    if (auto row = visibleRow()) {
        for (auto pageIndex : row->pages) {
            auto pageBounds = documentLayout.layoutBoundsForPageAtIndex(pageIndex);
            if (pageBounds.contains(point))
                return pageIndex;
        }

        return { };
    }

    for (PDFDocumentLayout::PageIndex pageIndex = 0; pageIndex < documentLayout.pageCount(); ++pageIndex) {
        auto pageBounds = documentLayout.layoutBoundsForPageAtIndex(pageIndex);
        if (pageBounds.contains(point))
            return pageIndex;
    }

    return { };
}

auto PDFPresentationController::pdfPositionForCurrentView(AnchorPoint anchorPoint, bool preservePosition) const -> std::optional<VisiblePDFPosition>
{
    if (!preservePosition)
        return { };

    CheckedRef checkedPlugin { m_plugin.get() };
    auto& documentLayout = checkedPlugin->documentLayout();
    if (!documentLayout.hasLaidOutPDFDocument())
        return { };

    auto maybePageIndex = pageIndexForCurrentView(anchorPoint);
    if (!maybePageIndex)
        return { };

    auto pageIndex = *maybePageIndex;
    auto pageBounds = documentLayout.layoutBoundsForPageAtIndex(pageIndex);
    auto topLeftInDocumentSpace = checkedPlugin->convertDown(UnifiedPDFPlugin::CoordinateSpace::Plugin, UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, FloatPoint::zero());
    auto pagePoint = documentLayout.documentToPDFPage(FloatPoint { pageBounds.center().x(), topLeftInDocumentSpace.y() }, pageIndex);

    LOG_WITH_STREAM(PDF, stream << "PDFPresentationController::pdfPositionForCurrentView - point " << pagePoint << " in page " << pageIndex << " with anchor point " << std::to_underlying(anchorPoint));

    return VisiblePDFPosition { pageIndex, pagePoint };
}

FloatPoint PDFPresentationController::anchorPointInDocumentSpace(AnchorPoint anchorPoint) const
{
    FloatPoint anchorPointInPluginSpace;
    CheckedRef checkedPlugin { m_plugin.get() };
    if (checkedPlugin->shouldSizeToFitContent()) {
        RefPtr view = checkedPlugin->frameView();
        if (!view)
            return { };

        anchorPointInPluginSpace = checkedPlugin->convertFromRootViewToPlugin([anchorPoint, view] -> FloatPoint {
            auto unobscuredRootViewRect = view->contentsToRootView(view->unobscuredContentRect());
            switch (anchorPoint) {
            case AnchorPoint::TopLeft:
                return unobscuredRootViewRect.location();
            case AnchorPoint::Center:
                return unobscuredRootViewRect.center();
            }
            ASSERT_NOT_REACHED();
            return { };
        }());
    } else {
        anchorPointInPluginSpace = [anchorPoint, checkedPlugin] -> FloatPoint {
            switch (anchorPoint) {
            case AnchorPoint::TopLeft:
                return { };
            case AnchorPoint::Center:
                return flooredIntPoint(checkedPlugin->size() / 2);
            }
            ASSERT_NOT_REACHED();
            return { };
        }();
    }
    return checkedPlugin->convertDown(UnifiedPDFPlugin::CoordinateSpace::Plugin, UnifiedPDFPlugin::CoordinateSpace::PDFDocumentLayout, anchorPointInPluginSpace);
}

bool PDFPresentationController::shouldUseInProcessBackingStore() const
{
    return m_plugin->shouldUseInProcessBackingStore();
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
