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

#include "PDFDocumentLayout.h"
#include "PDFPageCoverage.h"
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

OBJC_CLASS PDFDocument;

namespace WebCore {
enum class TiledBackingScrollability : uint8_t;
class GraphicsLayerClient;
class GraphicsLayer;
};

namespace WebKit {

class AsyncPDFRenderer;
class WebKeyboardEvent;
class UnifiedPDFPlugin;
class WebWheelEvent;
enum class RepaintRequirement : uint8_t;

class PDFPresentationController : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PDFPresentationController> {
    WTF_MAKE_NONCOPYABLE(PDFPresentationController);
    WTF_MAKE_TZONE_ALLOCATED(PDFPresentationController);
public:
    static RefPtr<PDFPresentationController> createForMode(PDFDocumentLayout::DisplayMode, UnifiedPDFPlugin&);

    PDFPresentationController(UnifiedPDFPlugin&);
    virtual ~PDFPresentationController();

    virtual WebCore::GraphicsLayerClient& graphicsLayerClient() = 0;

    // Subclasses must call the base class teardown().
    virtual void teardown();

    virtual bool supportsDisplayMode(PDFDocumentLayout::DisplayMode) const = 0;
    virtual void willChangeDisplayMode(PDFDocumentLayout::DisplayMode newMode) = 0;

    // Package up the data needed to paint a set of pages for the given clip, for use by UnifiedPDFPlugin::paintPDFContent and async rendering.
    virtual PDFPageCoverage pageCoverageForContentsRect(const WebCore::FloatRect&, std::optional<PDFLayoutRow>) const = 0;
    virtual PDFPageCoverageAndScales pageCoverageAndScalesForContentsRect(const WebCore::FloatRect&, std::optional<PDFLayoutRow>, float tilingScaleFactor) const = 0;

    virtual WebCore::FloatRect convertFromContentsToPainting(const WebCore::FloatRect&, std::optional<PDFDocumentLayout::PageIndex> = { }) const = 0;
    virtual WebCore::FloatRect convertFromPaintingToContents(const WebCore::FloatRect&, std::optional<PDFDocumentLayout::PageIndex> = { }) const = 0;

    virtual void deviceOrPageScaleFactorChanged() = 0;

    virtual void setupLayers(WebCore::GraphicsLayer&) = 0;
    virtual void updateLayersOnLayoutChange(WebCore::FloatSize documentSize, WebCore::FloatSize centeringOffset, double scaleFactor) = 0;

    virtual void updateIsInWindow(bool isInWindow) = 0;
    virtual void updateDebugBorders(bool showDebugBorders, bool showRepaintCounters) = 0;

    virtual void updateForCurrentScrollability(OptionSet<WebCore::TiledBackingScrollability>) = 0;

    virtual void didGeneratePreviewForPage(PDFDocumentLayout::PageIndex) = 0;

    virtual void repaintForIncrementalLoad() = 0;
    virtual void setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement>, const WebCore::FloatRect& rectInDocumentCoordinates, std::optional<PDFLayoutRow>) = 0;

    virtual std::optional<PDFLayoutRow> visibleRow() const { return { }; }
    virtual std::optional<PDFLayoutRow> rowForLayerID(WebCore::PlatformLayerIdentifier) const { return { }; }

    struct VisiblePDFPosition {
        PDFDocumentLayout::PageIndex pageIndex { 0 };
        WebCore::FloatPoint pagePoint;
    };

    virtual std::optional<VisiblePDFPosition> pdfPositionForCurrentView(bool preservePosition = true) const = 0;
    virtual void restorePDFPosition(const VisiblePDFPosition&) = 0;

    virtual void ensurePageIsVisible(PDFDocumentLayout::PageIndex) = 0;

    WebCore::FloatRect layoutBoundsForPageAtIndex(PDFDocumentLayout::PageIndex) const;

    PDFDocumentLayout::PageIndex nearestPageIndexForDocumentPoint(const WebCore::FloatPoint&) const;
    std::optional<PDFDocumentLayout::PageIndex> pageIndexForDocumentPoint(const WebCore::FloatPoint&) const;

    // Event handling.
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) = 0;
    virtual bool wantsWheelEvents() const { return false; }
    virtual bool handleWheelEvent(const WebWheelEvent&) { return false; }

    void releaseMemory();
    RetainPtr<PDFDocument> pluginPDFDocument() const;
    bool pluginShouldCachePagePreviews() const;

protected:
    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(const String&, WebCore::GraphicsLayer::Type);
    RefPtr<WebCore::GraphicsLayer> makePageContainerLayer(PDFDocumentLayout::PageIndex);

    static RefPtr<WebCore::GraphicsLayer> pageBackgroundLayerForPageContainerLayer(WebCore::GraphicsLayer&);

    Ref<AsyncPDFRenderer> asyncRenderer();
    RefPtr<AsyncPDFRenderer> asyncRendererIfExists() const;
    void clearAsyncRenderer();

    Ref<UnifiedPDFPlugin> m_plugin;
    RefPtr<AsyncPDFRenderer> m_asyncRenderer;

};

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
