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

#import "config.h"
#import "PDFDocumentLayout.h"

#if ENABLE(UNIFIED_PDF)

#import "Logging.h"
#import <WebCore/AffineTransform.h>
#import <WebCore/GeometryUtilities.h>
#import <wtf/text/TextStream.h>

#import "PDFKitSoftLink.h"

namespace WebKit {
using namespace WebCore;

static constexpr float minScale = 0.1; // Arbitrarily chosen min scale.

PDFDocumentLayout::PDFDocumentLayout() = default;
PDFDocumentLayout::~PDFDocumentLayout() = default;


bool PDFDocumentLayout::isLeftPageIndex(PageIndex pageIndex) const
{
    return !(pageIndex % 2);
}

bool PDFDocumentLayout::isRightPageIndex(PageIndex pageIndex) const
{
    return pageIndex % 2;
}

bool PDFDocumentLayout::isLastPageIndex(PageIndex pageIndex) const
{
    return pageIndex == pageCount() - 1;
}

RetainPtr<PDFPage> PDFDocumentLayout::pageAtIndex(PageIndex index) const
{
    return [m_pdfDocument pageAtIndex:index];
}

std::optional<unsigned> PDFDocumentLayout::indexForPage(RetainPtr<PDFPage> page) const
{
    for (unsigned pageIndex = 0; pageIndex < [m_pdfDocument pageCount]; ++pageIndex) {
        if (page == [m_pdfDocument pageAtIndex:pageIndex])
            return pageIndex;
    }
    return std::nullopt;
}

PDFDocumentLayout::PageIndex PDFDocumentLayout::nearestPageIndexForDocumentPoint(FloatPoint documentSpacePoint) const
{
    auto pageCount = this->pageCount();
    switch (displayMode()) {
    case PDFDocumentLayout::DisplayMode::TwoUpDiscrete:
    case PDFDocumentLayout::DisplayMode::TwoUpContinuous: {

        using PairedPagesLayoutBounds = std::pair<FloatRect, std::optional<FloatRect>>;

        auto layoutBoundsForPairedPages = [this](PageIndex pageIndex) -> PairedPagesLayoutBounds {
            if (isRightPageIndex(pageIndex))
                return { layoutBoundsForPageAtIndex(pageIndex - 1), layoutBoundsForPageAtIndex(pageIndex) };
            if (isLastPageIndex(pageIndex))
                return { layoutBoundsForPageAtIndex(pageIndex), { } };
            return { layoutBoundsForPageAtIndex(pageIndex), layoutBoundsForPageAtIndex(pageIndex + 1) };
        };

        auto minimumDistanceToPage = [documentSpacePoint](const FloatRect& pageLayoutBounds) {
            if (pageLayoutBounds.contains(documentSpacePoint))
                return 0.0f;

            auto pointsToCompare = Vector<FloatPoint> { pageLayoutBounds.minXMinYCorner(), pageLayoutBounds.minXMaxYCorner(), pageLayoutBounds.maxXMinYCorner(), pageLayoutBounds.maxXMaxYCorner() };

            bool isAbovePage = documentSpacePoint.y() < pageLayoutBounds.y();
            bool isBelowPage = documentSpacePoint.y() > pageLayoutBounds.maxY();
            bool isLeftOfPage = documentSpacePoint.x() < pageLayoutBounds.x();
            bool isRightOfPage = documentSpacePoint.x() > pageLayoutBounds.maxX();

            bool isWithinPageWidth = isInRange(documentSpacePoint.x(), pageLayoutBounds.x(), pageLayoutBounds.maxX());
            bool isWithinPageHeight = isInRange(documentSpacePoint.y(), pageLayoutBounds.y(), pageLayoutBounds.maxY());

            if (isAbovePage && isWithinPageWidth)
                pointsToCompare.append({ documentSpacePoint.x(), pageLayoutBounds.y() });
            else if (isRightOfPage && isWithinPageHeight)
                pointsToCompare.append({ pageLayoutBounds.x(), documentSpacePoint.y() });
            else if (isBelowPage && isWithinPageWidth)
                pointsToCompare.append({ documentSpacePoint.x(), pageLayoutBounds.maxY() });
            else if (isLeftOfPage && isWithinPageHeight)
                pointsToCompare.append({ pageLayoutBounds.x(), documentSpacePoint.y() });

            auto distancesToPoints = WTF::map(pointsToCompare, [documentSpacePoint](FloatPoint point) {
                return euclidianDistance(point, documentSpacePoint);
            });

            return *std::min_element(distancesToPoints.begin(), distancesToPoints.end());
        };

        for (PageIndex index = 0; index < pageCount; index += pagesPerRow()) {
            auto [leftPageLayoutBounds, rightPageLayoutBounds] = layoutBoundsForPairedPages(index);

            if (documentSpacePoint.y() < leftPageLayoutBounds.maxY() || (rightPageLayoutBounds && documentSpacePoint.y() < rightPageLayoutBounds->maxY())) {

                if (!rightPageLayoutBounds || leftPageLayoutBounds.contains(documentSpacePoint))
                    return index;

                if (rightPageLayoutBounds->contains(documentSpacePoint))
                    return index + 1;

                if (minimumDistanceToPage(leftPageLayoutBounds) < minimumDistanceToPage(rightPageLayoutBounds.value()))
                    return index;

                return index + 1;
            }
        }

        auto lastPageIndex = pageCount - 1;
        if (isLeftPageIndex(lastPageIndex))
            return lastPageIndex;

        auto [leftPageLayoutBounds, rightPageLayoutBounds] = layoutBoundsForPairedPages(lastPageIndex);
        ASSERT(rightPageLayoutBounds);

        if (minimumDistanceToPage(leftPageLayoutBounds) < minimumDistanceToPage(rightPageLayoutBounds.value_or(FloatRect { })))
            return lastPageIndex - 1;

        return lastPageIndex;
    }
    case PDFDocumentLayout::DisplayMode::SinglePageDiscrete:
    case PDFDocumentLayout::DisplayMode::SinglePageContinuous: {
        for (PDFDocumentLayout::PageIndex index = 0; index < pageCount; ++index) {
            auto pageBounds = layoutBoundsForPageAtIndex(index);

            if (documentSpacePoint.y() <= pageBounds.maxY() || index == pageCount - 1)
                return index;
        }
    }
    }
    ASSERT_NOT_REACHED();
    return pageCount - 1;
}

void PDFDocumentLayout::updateLayout(IntSize pluginSize, ShouldUpdateAutoSizeScale shouldUpdateScale)
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

    float maxRowWidth = 0;
    float currentRowWidth = 0;
    bool isTwoUpLayout = m_displayMode == DisplayMode::TwoUpDiscrete || m_displayMode == DisplayMode::TwoUpContinuous;

    for (PageIndex i = 0; i < pageCount; ++i) {
        auto page = pageAtIndex(i);
        if (!page) {
            m_pageGeometry.append({ });
            continue;
        }

        auto pageCropBox = FloatRect { [page boundsForBox:kPDFDisplayBoxCropBox] };
        auto rotation = normalizeRotation([page rotation]);

        LOG_WITH_STREAM(PDF, stream << "PDFDocumentLayout::updateLayout() - page " << i << " crop box " << pageCropBox << " rotation " << rotation);

        auto pageBounds = normalizePageBounds(pageCropBox, rotation);

        if (isTwoUpLayout) {
            if (i % 2) {
                currentRowWidth += pageMargin.width() + pageBounds.width();
                maxRowWidth = std::max(maxRowWidth, currentRowWidth);
            } else {
                currentRowWidth = pageBounds.width();
                if (i == pageCount - 1)
                    maxRowWidth = std::max(maxRowWidth, currentRowWidth);
            }
        } else
            maxRowWidth = std::max(maxRowWidth, pageBounds.width());

        m_pageGeometry.append({ pageCropBox, pageBounds, rotation });
    }

    maxRowWidth += 2 * documentMargin.width();

    layoutPages(pluginSize.width(), maxRowWidth, shouldUpdateScale);

    LOG_WITH_STREAM(PDF, stream << "PDFDocumentLayout::updateLayout() - plugin size " << pluginSize << " document bounds " << m_documentBounds << " scale " << m_scale);
}

void PDFDocumentLayout::layoutPages(float availableWidth, float maxRowWidth, ShouldUpdateAutoSizeScale shouldUpdateScale)
{
    // We always lay out in a continuous mode. We handle non-continuous mode via scroll snap.
    switch (m_displayMode) {
    case DisplayMode::SinglePageDiscrete:
    case DisplayMode::SinglePageContinuous:
        layoutSingleColumn(availableWidth, maxRowWidth, shouldUpdateScale);
        break;

    case DisplayMode::TwoUpDiscrete:
    case DisplayMode::TwoUpContinuous:
        layoutTwoUpColumn(availableWidth, maxRowWidth, shouldUpdateScale);
        break;
    }
}

void PDFDocumentLayout::layoutSingleColumn(float availableWidth, float maxRowWidth, ShouldUpdateAutoSizeScale shouldUpdateScale)
{
    float currentYOffset = documentMargin.height();
    auto pageCount = this->pageCount();

    for (PageIndex i = 0; i < pageCount; ++i) {
        if (i >= m_pageGeometry.size())
            break;

        auto pageBounds = m_pageGeometry[i].layoutBounds;

        LOG_WITH_STREAM(PDF, stream << "PDFDocumentLayout::layoutSingleColumn - page " << i << " bounds " << pageBounds);

        auto pageLeft = std::max<float>(std::floor((maxRowWidth - pageBounds.width()) / 2), 0);
        pageBounds.setLocation({ pageLeft, currentYOffset });

        currentYOffset += pageBounds.height() + pageMargin.height();

        m_pageGeometry[i].layoutBounds = pageBounds;
    }

    currentYOffset -= pageMargin.height();
    currentYOffset += documentMargin.height();

    if (shouldUpdateScale == ShouldUpdateAutoSizeScale::Yes)
        m_scale = std::max<float>(availableWidth / maxRowWidth, minScale);
    m_documentBounds = FloatRect { 0, 0, maxRowWidth, currentYOffset };

    LOG_WITH_STREAM(PDF, stream << "PDFDocumentLayout::layoutSingleColumn - document bounds " << m_documentBounds << " scale " << m_scale);
}

void PDFDocumentLayout::layoutTwoUpColumn(float availableWidth, float maxRowWidth, ShouldUpdateAutoSizeScale shouldUpdateScale)
{
    FloatSize currentRowSize;
    float currentYOffset = documentMargin.height();
    auto pageCount = this->pageCount();

    for (PageIndex i = 0; i < pageCount; ++i) {
        if (i >= m_pageGeometry.size())
            break;

        auto pageBounds = m_pageGeometry[i].layoutBounds;

        // Lay out the pages in pairs.
        if (i % 2) {
            currentRowSize.expand(pageMargin.width() + pageBounds.width(), 0);
            currentRowSize.setHeight(std::max(currentRowSize.height(), pageBounds.height()));

            auto leftPageBounds = m_pageGeometry[i - 1].layoutBounds;
            auto rightPageBounds = pageBounds;

            // Center each page vertically in the row.
            // Center the pair of pages horizontally.
            float horizontalSpace = maxRowWidth - 2 * documentMargin.width() - leftPageBounds.width() - rightPageBounds.width();
            leftPageBounds.setX(std::floor(documentMargin.width() + horizontalSpace / 2));
            rightPageBounds.setX(leftPageBounds.maxX() + pageMargin.width());

            float leftVerticalSpace = currentRowSize.height() - leftPageBounds.height();
            leftPageBounds.setY(currentYOffset + std::floor(leftVerticalSpace / 2));

            float rightVerticalSpace = currentRowSize.height() - rightPageBounds.height();
            rightPageBounds.setY(currentYOffset + std::floor(rightVerticalSpace / 2));

            m_pageGeometry[i - 1].layoutBounds = leftPageBounds;
            m_pageGeometry[i].layoutBounds = rightPageBounds;

            currentYOffset += currentRowSize.height() + pageMargin.height();
        } else {
            currentRowSize = pageBounds.size();
            if (i == pageCount - 1) {
                // Position the last page, which is centered horizontally.
                float horizontalSpace = maxRowWidth - 2 * documentMargin.width() - pageBounds.width();
                m_pageGeometry[i].layoutBounds.setLocation({ documentMargin.width() + std::floor(horizontalSpace / 2), currentYOffset });
                currentYOffset += currentRowSize.height() + pageMargin.height();
            }
        }
    }

    // Subtract the last row's bottom margin.
    currentYOffset -= pageMargin.height();
    currentYOffset += documentMargin.height();

    if (shouldUpdateScale == ShouldUpdateAutoSizeScale::Yes)
        m_scale = std::max<float>(availableWidth / maxRowWidth, minScale);
    m_documentBounds = FloatRect { 0, 0, maxRowWidth, currentYOffset };
}

size_t PDFDocumentLayout::pageCount() const
{
    if (!m_pdfDocument)
        return 0;

    return [m_pdfDocument pageCount];
}

FloatRect PDFDocumentLayout::layoutBoundsForPageAtIndex(PageIndex index) const
{
    if (index >= m_pageGeometry.size())
        return { };

    return m_pageGeometry[index].layoutBounds;
}

IntDegrees PDFDocumentLayout::rotationForPageAtIndex(PageIndex index) const
{
    if (index >= m_pageGeometry.size())
        return 0;

    return m_pageGeometry[index].rotation;
}

FloatSize PDFDocumentLayout::scaledContentsSize() const
{
    return m_documentBounds.size().scaled(m_scale);
}

auto PDFDocumentLayout::geometryForPage(RetainPtr<PDFPage> page) const -> std::optional<PageGeometry>
{
    if (auto pageIndex = indexForPage(page))
        return m_pageGeometry[*pageIndex];
    return { };
}

AffineTransform PDFDocumentLayout::toPageTransform(const PageGeometry& pageGeometry) const
{
    AffineTransform matrix;
    switch (pageGeometry.rotation) {
    default:
        FALLTHROUGH;
    case 0:
        matrix = AffineTransform::makeTranslation(FloatSize { pageGeometry.cropBox.x(), pageGeometry.cropBox.y() });
        break;
    case 90:
        matrix = AffineTransform::makeRotation(pageGeometry.rotation);
        matrix.translate(pageGeometry.cropBox.y(), -pageGeometry.cropBox.width() - pageGeometry.cropBox.x());
        break;
    case 180:
        matrix = AffineTransform::makeRotation(pageGeometry.rotation);
        matrix.translate(-pageGeometry.cropBox.width() - pageGeometry.cropBox.x(), -pageGeometry.cropBox.height() - pageGeometry.cropBox.y());
        break;
    case 270:
        matrix = AffineTransform::makeRotation(pageGeometry.rotation);
        matrix.translate(-pageGeometry.cropBox.height() - pageGeometry.cropBox.y(), pageGeometry.cropBox.x());
        break;
    }
    return matrix;
}

FloatPoint PDFDocumentLayout::documentToPDFPage(FloatPoint documentPoint, PageIndex pageIndex) const
{
    if (pageIndex >= m_pageGeometry.size())
        return documentPoint;

    auto& pageGeometry = m_pageGeometry[pageIndex];

    auto mappedPoint = documentPoint;
    mappedPoint.moveBy(-pageGeometry.layoutBounds.location());

    mappedPoint.setY(pageGeometry.layoutBounds.height() - mappedPoint.y());

    auto matrix = toPageTransform(pageGeometry);
    mappedPoint = matrix.mapPoint(mappedPoint);
    return mappedPoint;
}

FloatRect PDFDocumentLayout::documentToPDFPage(FloatRect documentRect, PageIndex pageIndex) const
{
    if (pageIndex >= m_pageGeometry.size())
        return documentRect;

    auto& pageGeometry = m_pageGeometry[pageIndex];

    auto mappedRect = documentRect;

    // FIXME: Possibly wrong.
    mappedRect.moveBy(-pageGeometry.layoutBounds.location());
    mappedRect.setY(pageGeometry.layoutBounds.height() - mappedRect.y());

    auto matrix = toPageTransform(pageGeometry);
    mappedRect = matrix.mapRect(mappedRect);
    return mappedRect;
}

FloatPoint PDFDocumentLayout::pdfPageToDocument(FloatPoint pagePoint, PageIndex pageIndex) const
{
    if (pageIndex >= m_pageGeometry.size())
        return pagePoint;

    auto& pageGeometry = m_pageGeometry[pageIndex];

    auto matrix = toPageTransform(pageGeometry);
    auto mappedPoint = matrix.inverse().value_or(AffineTransform { }).mapPoint(pagePoint);

    mappedPoint.setY(pageGeometry.layoutBounds.height() - mappedPoint.y());
    mappedPoint.moveBy(pageGeometry.layoutBounds.location());

    return mappedPoint;
}

FloatRect PDFDocumentLayout::pdfPageToDocument(const FloatRect pageSpaceRect, PageIndex pageIndex) const
{
    if (pageIndex >= m_pageGeometry.size())
        return pageSpaceRect;

    auto& pageGeometry = m_pageGeometry[pageIndex];

    auto matrix = toPageTransform(pageGeometry);
    auto mappedRect = matrix.inverse().value_or(AffineTransform { }).mapRect(pageSpaceRect);

    mappedRect.setY(pageGeometry.layoutBounds.height() - mappedRect.y() - mappedRect.height());
    mappedRect.moveBy(pageGeometry.layoutBounds.location());

    return mappedRect;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
