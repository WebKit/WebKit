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
#include "PDFDiscretePresentationController.h"
#include "PDFKitSPI.h"
#include "PDFScrollingPresentationController.h"
#include <WebCore/GraphicsLayer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PDFPresentationController);

RefPtr<PDFPresentationController> PDFPresentationController::createForMode(PDFDocumentLayout::DisplayMode mode, UnifiedPDFPlugin& plugin)
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

RefPtr<GraphicsLayer> PDFPresentationController::makePageContainerLayer(PDFDocumentLayout::PageIndex pageIndex)
{
    auto addLayerShadow = [](GraphicsLayer& layer, IntPoint shadowOffset, const Color& shadowColor, int shadowStdDeviation) {
        Vector<Ref<FilterOperation>> filterOperations;
        filterOperations.append(DropShadowFilterOperation::create(shadowOffset, shadowStdDeviation, shadowColor));
        layer.setFilters(FilterOperations { WTFMove(filterOperations) });
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
    addLayerShadow(*pageContainerLayer, containerShadowOffset, containerShadowColor, containerShadowStdDeviation);

    pageBackgroundLayer->setAnchorPoint({ });
    pageBackgroundLayer->setBackgroundColor(Color::white);

    pageBackgroundLayer->setDrawsContent(true);
    pageBackgroundLayer->setAcceleratesDrawing(true);
    pageBackgroundLayer->setShouldUpdateRootRelativeScaleFactor(false);
    pageBackgroundLayer->setNeedsDisplay(); // We only need to paint this layer once when page backgrounds change.

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
        return WTFMove(layer);
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

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
