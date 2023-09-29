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

#include "config.h"
#include "PDFDocumentLayout.h"

#if ENABLE(UNIFIED_PDF)

#include <CoreGraphics/CoreGraphics.h>

namespace WebKit {
using namespace WebCore;

FloatSize PDFDocumentLayout::documentMargin()
{
    return { 20, 20 };
}

FloatSize PDFDocumentLayout::pageMargin()
{
    return { 10, 10 };
}

PDFDocumentLayout::PDFDocumentLayout() = default;
PDFDocumentLayout::~PDFDocumentLayout() = default;

void PDFDocumentLayout::setPDFDocument(RetainPtr<CGPDFDocumentRef>&& pdfDocument)
{
    m_pdfDocument = WTFMove(pdfDocument);
    updateGeometry();
}

bool PDFDocumentLayout::hasPDFDocument() const
{
    return !!m_pdfDocument;
}

RetainPtr<CGPDFPageRef> PDFDocumentLayout::pageAtIndex(PageIndex index) const
{
    RetainPtr page = CGPDFDocumentGetPage(m_pdfDocument.get(), index + 1); // CG Page index is 1-based
    return page;
}

void PDFDocumentLayout::updateGeometry()
{
    auto pageCount = this->pageCount();
    m_pageBounds.clear();
    m_documentBounds = { };

    for (PageIndex i = 0; i < pageCount; ++i) {
        auto page = pageAtIndex(i);
        if (!page) {
            m_pageBounds.append({ });
            continue;
        }

        auto pageCropBox = FloatRect { CGPDFPageGetBoxRect(page.get(), kCGPDFCropBox) };

        // FIXME: Handle page rotation
        m_pageBounds.append(pageCropBox);
    }

    float availableWidth = 1000; // FIXME: Fixed for now, but should use plugin width, maybe with a second layout pass when horizontal scrolling is required.
    layoutPages(availableWidth);

    // FIXME: Update plugin size here.
}

void PDFDocumentLayout::layoutPages(float availableWidth)
{
    // We always lay out in a continuous mode. We handle non-continuous mode via scroll snap.
    switch (m_displayMode) {
    case DisplayMode::SinglePage:
    case DisplayMode::Continuous:
        layoutSingleColumn(availableWidth);
        break;

    case DisplayMode::TwoUp:
    case DisplayMode::TwoUpContinuous:
        layoutTwoUpColumn(availableWidth);
        break;
    }
}

void PDFDocumentLayout::layoutSingleColumn(float availableWidth)
{
    auto documentMargin = PDFDocumentLayout::documentMargin();
    auto pageMargin = PDFDocumentLayout::documentMargin();

    float currentYOffset = documentMargin.height();
    float maxPageWidth = 0;
    auto pageCount = this->pageCount();

    for (PageIndex i = 0; i < pageCount; ++i) {
        auto pageBounds = m_pageBounds[i];

        auto pageLeft = std::max<float>(std::floor((availableWidth - pageBounds.width()) / 2), 0);
        pageBounds.setLocation({ pageLeft, currentYOffset });

        currentYOffset += pageBounds.height() + pageMargin.height();
        maxPageWidth = std::max(maxPageWidth, pageBounds.width());

        m_pageBounds[i] = pageBounds;
    }

    // Subtract the last page's bottom margin.
    currentYOffset -= pageMargin.height();
    currentYOffset += documentMargin.height();

    m_documentBounds = FloatRect { 0, 0, std::max(maxPageWidth, availableWidth), currentYOffset };
}

void PDFDocumentLayout::layoutTwoUpColumn(float availableWidth)
{
    // FIMXE: Not implemented yet.
}

size_t PDFDocumentLayout::pageCount() const
{
    if (!hasPDFDocument())
        return 0;

    return CGPDFDocumentGetNumberOfPages(m_pdfDocument.get());
}

WebCore::FloatRect PDFDocumentLayout::boundsForPageAtIndex(PageIndex index) const
{
    if (index >= m_pageBounds.size())
        return { };

    return m_pageBounds[index];
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
