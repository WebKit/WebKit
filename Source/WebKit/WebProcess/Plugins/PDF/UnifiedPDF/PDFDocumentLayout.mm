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

#include "Logging.h"
#include <CoreGraphics/CoreGraphics.h>
#include <wtf/text/TextStream.h>

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

void PDFDocumentLayout::updateLayout(IntSize pluginSize)
{
    auto pageCount = this->pageCount();
    m_pageGeometry.clear();
    m_documentBounds = { };

    auto normalizeRotation = [](IntDegrees degrees) {
        if (degrees < 0)
            degrees += 360 * (1 + (degrees / -360));

        // Round to nearest 90 degree angle
        degrees = std::round(static_cast<double>(degrees) / 90.0) * 90.0;

        // Normalize in positive space
        return degrees % 360;
    };

    auto normalizePageBounds = [&](FloatRect cropBox, int degrees) {
        auto r = cropBox;
        if (degrees == 90 || degrees == 270)
            r.setSize(r.size().transposedSize());
        return r;
    };

    float maxPageWidth = 0;

    for (PageIndex i = 0; i < pageCount; ++i) {
        auto page = pageAtIndex(i);
        if (!page) {
            m_pageGeometry.append({ });
            continue;
        }

        auto pageCropBox = FloatRect { CGPDFPageGetBoxRect(page.get(), kCGPDFCropBox) };
        auto rotation = normalizeRotation(CGPDFPageGetRotationAngle(page.get()));

        LOG_WITH_STREAM(Plugins, stream << "PDFDocumentLayout::updateLayout() - page " << i << " crop box " << pageCropBox << " rotation " << rotation);

        auto pageBounds = normalizePageBounds(pageCropBox, rotation);
        maxPageWidth = std::max(maxPageWidth, pageBounds.width());

        m_pageGeometry.append({ pageBounds, rotation });
    }

    layoutPages(pluginSize.width(), maxPageWidth);

    LOG_WITH_STREAM(Plugins, stream << "PDFDocumentLayout::updateLayout() - plugin size " << pluginSize << " document bounds " << m_documentBounds << " scale " << m_scale);
}

void PDFDocumentLayout::layoutPages(float availableWidth, float maxPageWidth)
{
    // We always lay out in a continuous mode. We handle non-continuous mode via scroll snap.
    switch (m_displayMode) {
    case DisplayMode::SinglePage:
    case DisplayMode::Continuous:
        layoutSingleColumn(availableWidth, maxPageWidth);
        break;

    case DisplayMode::TwoUp:
    case DisplayMode::TwoUpContinuous:
        layoutTwoUpColumn(availableWidth);
        break;
    }
}

void PDFDocumentLayout::layoutSingleColumn(float availableWidth, float maxPageWidth)
{
    auto documentMargin = PDFDocumentLayout::documentMargin();
    auto pageMargin = PDFDocumentLayout::pageMargin();

    float currentYOffset = documentMargin.height();
    auto pageCount = this->pageCount();

    maxPageWidth += 2 * documentMargin.width();

    for (PageIndex i = 0; i < pageCount; ++i) {
        if (i >= m_pageGeometry.size())
            break;

        auto pageBounds = m_pageGeometry[i].normalizedBounds;

        auto pageLeft = std::max<float>(std::floor((maxPageWidth - pageBounds.width()) / 2), 0);
        pageBounds.setLocation({ pageLeft, currentYOffset });

        currentYOffset += pageBounds.height() + pageMargin.height();

        m_pageGeometry[i].normalizedBounds = pageBounds;
    }

    // Subtract the last page's bottom margin.
    currentYOffset -= pageMargin.height();
    currentYOffset += documentMargin.height();

    constexpr float minScale = 0.1; // Arbitrarily chosen min scale.
    m_scale = std::max<float>(availableWidth / maxPageWidth, minScale);
    m_documentBounds = FloatRect { 0, 0, availableWidth, currentYOffset };
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
    if (index >= m_pageGeometry.size())
        return { };

    return m_pageGeometry[index].normalizedBounds;
}

IntDegrees PDFDocumentLayout::rotationForPageAtIndex(PageIndex index) const
{
    if (index >= m_pageGeometry.size())
        return 0;

    return m_pageGeometry[index].rotation;
}

WebCore::FloatSize PDFDocumentLayout::scaledContentsSize() const
{
    return m_documentBounds.size().scaled(m_scale);
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
