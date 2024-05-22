/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <WebCore/FloatRect.h>
#include <WebCore/IntDegrees.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS PDFDocument;
OBJC_CLASS PDFPage;

namespace WebCore {
class GraphicsContext;
class IntRect;
}

namespace WebKit {

enum class ShouldUpdateAutoSizeScale : bool { No, Yes };

struct PDFLayoutRow;

class PDFDocumentLayout {
public:
    using PageIndex = size_t; // This is a zero-based index.

    enum class DisplayMode : uint8_t {
        SinglePageDiscrete,
        SinglePageContinuous,
        TwoUpDiscrete,
        TwoUpContinuous,
    };

    PDFDocumentLayout();
    ~PDFDocumentLayout();

    void setPDFDocument(PDFDocument *document) { m_pdfDocument = document; }
    bool hasPDFDocument() const { return !!m_pdfDocument; }
    bool hasLaidOutPDFDocument() const { return !m_pageGeometry.isEmpty(); }

    size_t pageCount() const;
    size_t rowCount() const;
    PDFLayoutRow rowForPageIndex(PageIndex) const;
    Vector<PDFLayoutRow> rows() const;
    unsigned rowIndexForPageIndex(PageIndex) const;

    static constexpr WebCore::FloatSize documentMargin { 6, 8 };
    static constexpr WebCore::FloatSize pageMargin { 4, 6 };

    bool isLeftPageIndex(PageIndex) const;
    bool isRightPageIndex(PageIndex) const;
    bool isLastPageIndex(PageIndex) const;
    PageIndex lastPageIndex() const;
    bool isFirstPageOfRow(PageIndex) const;

    RetainPtr<PDFPage> pageAtIndex(PageIndex) const;
    std::optional<PageIndex> indexForPage(RetainPtr<PDFPage>) const;
    PageIndex nearestPageIndexForDocumentPoint(WebCore::FloatPoint, std::optional<PDFLayoutRow>) const;
    // For the given Y offset, return a page index and page point for the page at this offset. Returns the leftmost
    // page if two-up and both pages intersect that offset, otherwise the right page if only it intersects the offset.
    // The point is centered horizontally in the given page.
    std::pair<PageIndex, WebCore::FloatPoint> pageIndexAndPagePointForDocumentYOffset(float) const;

    // This is not scaled by scale().
    WebCore::FloatRect layoutBoundsForPageAtIndex(PageIndex) const;
    // Bounds of the pages in the row, including document margins. Not scaled.
    WebCore::FloatRect layoutBoundsForRow(PDFLayoutRow) const;

    // Returns 0, 90, 180, 270.
    WebCore::IntDegrees rotationForPageAtIndex(PageIndex) const;

    WebCore::FloatPoint documentToPDFPage(WebCore::FloatPoint documentPoint, PageIndex) const;
    WebCore::FloatRect documentToPDFPage(WebCore::FloatRect documentRect, PageIndex) const;

    WebCore::FloatPoint pdfPageToDocument(WebCore::FloatPoint pagePoint, PageIndex) const;
    WebCore::FloatRect pdfPageToDocument(WebCore::FloatRect pageRect, PageIndex) const;

    // This is the scale that scales the largest page or pair of pages up or down to fit the available width.
    float scale() const { return m_scale; }

    void updateLayout(WebCore::IntSize pluginSize, ShouldUpdateAutoSizeScale);
    WebCore::FloatSize contentsSize() const;
    WebCore::FloatSize scaledContentsSize() const;

    void setDisplayMode(DisplayMode displayMode) { m_displayMode = displayMode; }
    DisplayMode displayMode() const { return m_displayMode; }

    constexpr static bool isSinglePageDisplayMode(DisplayMode mode) { return mode == DisplayMode::SinglePageDiscrete || mode == DisplayMode::SinglePageContinuous; }
    constexpr static bool isTwoUpDisplayMode(DisplayMode mode) { return mode == DisplayMode::TwoUpDiscrete || mode == DisplayMode::TwoUpContinuous; }

    constexpr static bool isScrollingDisplayMode(DisplayMode mode) { return mode == DisplayMode::SinglePageContinuous || mode == DisplayMode::TwoUpContinuous; }
    constexpr static bool isDiscreteDisplayMode(DisplayMode mode) { return mode == DisplayMode::SinglePageDiscrete || mode == DisplayMode::TwoUpDiscrete; }

    bool isSinglePageDisplayMode() const { return isSinglePageDisplayMode(m_displayMode); }
    bool isTwoUpDisplayMode() const { return isTwoUpDisplayMode(m_displayMode); }

    bool isScrollingDisplayMode() const { return isScrollingDisplayMode(m_displayMode); }
    bool isDiscreteDisplayMode() const { return isDiscreteDisplayMode(m_displayMode); }

    unsigned pagesPerRow() const { return isSinglePageDisplayMode() ? 1 : 2; }

    struct PageGeometry {
        WebCore::FloatRect cropBox;
        WebCore::FloatRect layoutBounds;
        WebCore::IntDegrees rotation { 0 };
    };

    std::optional<PageGeometry> geometryForPage(RetainPtr<PDFPage>) const;
    WebCore::AffineTransform toPageTransform(const PageGeometry&) const;

private:
    void layoutPages(float availableWidth, float maxRowWidth, ShouldUpdateAutoSizeScale);

    void layoutSingleColumn(float availableWidth, float maxRowWidth, ShouldUpdateAutoSizeScale);
    void layoutTwoUpColumn(float availableWidth, float maxRowWidth, ShouldUpdateAutoSizeScale);

    RetainPtr<PDFDocument> m_pdfDocument;
    Vector<PageGeometry> m_pageGeometry;
    WebCore::FloatRect m_documentBounds;
    float m_scale { 1 };
    DisplayMode m_displayMode { DisplayMode::SinglePageContinuous };
};

struct PDFLayoutRow {
    Vector<PDFDocumentLayout::PageIndex, 2> pages;
    unsigned numPages() const { return pages.size(); }
    bool containsPage(PDFDocumentLayout::PageIndex pageIndex) const { return pages.contains(pageIndex); }
};

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
