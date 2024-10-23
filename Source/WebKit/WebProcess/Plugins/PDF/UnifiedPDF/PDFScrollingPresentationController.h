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

#pragma once

#if ENABLE(UNIFIED_PDF)

#include "PDFPresentationController.h"
#include <WebCore/GraphicsLayerClient.h>
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class KeyboardScrollingAnimator;
};

namespace WebKit {

class PDFScrollingPresentationController final : public PDFPresentationController, public WebCore::GraphicsLayerClient {
    WTF_MAKE_TZONE_ALLOCATED(PDFScrollingPresentationController);
    WTF_MAKE_NONCOPYABLE(PDFScrollingPresentationController);
public:
    PDFScrollingPresentationController(UnifiedPDFPlugin&);


private:
    bool supportsDisplayMode(PDFDocumentLayout::DisplayMode) const override;
    void willChangeDisplayMode(PDFDocumentLayout::DisplayMode) override { }

    void teardown() override;

    PDFPageCoverage pageCoverageForContentsRect(const WebCore::FloatRect&, std::optional<PDFLayoutRow>) const override;
    PDFPageCoverageAndScales pageCoverageAndScalesForContentsRect(const WebCore::FloatRect&, std::optional<PDFLayoutRow>, float tilingScaleFactor) const override;

    WebCore::FloatRect convertFromContentsToPainting(const WebCore::FloatRect& rect, std::optional<PDFDocumentLayout::PageIndex>) const override { return rect; }
    WebCore::FloatRect convertFromPaintingToContents(const WebCore::FloatRect& rect, std::optional<PDFDocumentLayout::PageIndex>) const override { return rect; }

    void deviceOrPageScaleFactorChanged() override { }

    void setupLayers(WebCore::GraphicsLayer& scrolledContentsLayer) override;
    void updateLayersOnLayoutChange(WebCore::FloatSize documentSize, WebCore::FloatSize centeringOffset, double scaleFactor) override;

    void updateIsInWindow(bool isInWindow) override;
    void updateDebugBorders(bool showDebugBorders, bool showRepaintCounters) override;
    void updateForCurrentScrollability(OptionSet<WebCore::TiledBackingScrollability>) override;

    GraphicsLayerClient& graphicsLayerClient() override { return *this; }

    bool handleKeyboardEvent(const WebKeyboardEvent&) override;
#if PLATFORM(MAC)
    bool handleKeyboardCommand(const WebKeyboardEvent&);
    CheckedPtr<WebCore::KeyboardScrollingAnimator> checkedKeyboardScrollingAnimator() const;
#endif

    std::optional<VisiblePDFPosition> pdfPositionForCurrentView(bool preservePosition) const override;
    void restorePDFPosition(const VisiblePDFPosition&) override;

    void ensurePageIsVisible(PDFDocumentLayout::PageIndex) override { }

    // GraphicsLayerClient
    void notifyFlushRequired(const WebCore::GraphicsLayer*) override;
    float pageScaleFactor() const override;
    float deviceScaleFactor() const override;
    std::optional<float> customContentsScale(const WebCore::GraphicsLayer*) const override;
    bool layerNeedsPlatformContext(const WebCore::GraphicsLayer*) const override;
    void tiledBackingUsageChanged(const WebCore::GraphicsLayer*, bool /*usingTiledBacking*/) override;
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect&, OptionSet<WebCore::GraphicsLayerPaintBehavior>) override;

#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    void paintPDFSelection(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect, std::optional<PDFLayoutRow> = { });
#endif

    void updatePageBackgroundLayers();
    std::optional<PDFDocumentLayout::PageIndex> pageIndexForPageBackgroundLayer(const WebCore::GraphicsLayer*) const;
    WebCore::GraphicsLayer* backgroundLayerForPage(PDFDocumentLayout::PageIndex) const;

    void didGeneratePreviewForPage(PDFDocumentLayout::PageIndex) override;

    void paintBackgroundLayerForPage(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect, PDFDocumentLayout::PageIndex);

    void repaintForIncrementalLoad() override;
    void setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement>, const WebCore::FloatRect& rectInDocumentCoordinates, std::optional<PDFLayoutRow>) override;

    RefPtr<WebCore::GraphicsLayer> m_pageBackgroundsContainerLayer;
    RefPtr<WebCore::GraphicsLayer> m_contentsLayer;
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    RefPtr<WebCore::GraphicsLayer> m_selectionLayer;
#endif

    HashMap<RefPtr<WebCore::GraphicsLayer>, PDFDocumentLayout::PageIndex> m_pageBackgroundLayers;
};


} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
