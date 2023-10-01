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

#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/FloatRect.h>
#include <WebCore/IntDegrees.h>
#include <wtf/RetainPtr.h>

namespace WebCore {
class GraphicsContext;
class IntRect;
}

namespace WebKit {

class PDFDocumentLayout {
public:
    using PageIndex = size_t; // This is a zero-based index.

    enum class DisplayMode : uint8_t {
        SinglePage,
        Continuous,
        TwoUp,
        TwoUpContinuous,
    };

    PDFDocumentLayout();
    ~PDFDocumentLayout();

    void setPDFDocument(RetainPtr<CGPDFDocumentRef>&&);
    CGPDFDocumentRef pdfDocument() const { return m_pdfDocument.get(); }

    bool hasPDFDocument() const;
    size_t pageCount() const;

    RetainPtr<CGPDFPageRef> pageAtIndex(PageIndex) const;
    WebCore::FloatRect boundsForPageAtIndex(PageIndex) const;
    // Returns 0, 90, 180, 270.
    IntDegrees rotationForPageAtIndex(PageIndex) const;

    WebCore::FloatSize layoutSize() const { return m_documentBounds.size(); }

private:

    void updateGeometry();

    void layoutPages(float availableWidth);

    void layoutSingleColumn(float availableWidth);
    void layoutTwoUpColumn(float availableWidth);

    static FloatSize documentMargin();
    static FloatSize pageMargin();

    struct PageGeometry {
        WebCore::FloatRect normalizedBounds;
        IntDegrees rotation { 0 };
    };

    RetainPtr<CGPDFDocumentRef> m_pdfDocument;
    Vector<PageGeometry> m_pageGeometry;
    WebCore::FloatRect m_documentBounds;
    DisplayMode m_displayMode { DisplayMode::Continuous };
};

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
