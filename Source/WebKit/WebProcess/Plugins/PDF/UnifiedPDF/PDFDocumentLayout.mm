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
    ASSERT(isTwoUpDisplayMode());
    return pageIndex % 2;
}

bool PDFDocumentLayout::isLastPageIndex(PageIndex pageIndex) const
{
    return pageIndex == lastPageIndex();
}

auto PDFDocumentLayout::lastPageIndex() const -> PageIndex
{
    ASSERT(pageCount());
    if (!pageCount())
        return 0;

    return pageCount() - 1;
}

bool PDFDocumentLayout::isFirstPageOfRow(PageIndex pageIndex) const
{
    return isTwoUpDisplayMode() ? isLeftPageIndex(pageIndex) : true;
}

RetainPtr<PDFPage> PDFDocumentLayout::pageAtIndex(PageIndex index) const
{
    return [m_pdfDocument pageAtIndex:index];
}

auto PDFDocumentLayout::indexForPage(RetainPtr<PDFPage> page) const -> std::optional<PageIndex>
{

    auto pageIndex = [m_pdfDocument indexForPage:page.get()];
    if (pageIndex == NSNotFound)
        return { };
    return pageIndex;
}

PDFDocumentLayout::PageIndex PDFDocumentLayout::nearestPageIndexForDocumentPoint(FloatPoint documentSpacePoint, const std::optional<PDFLayoutRow>& visibleRow) const
{
    auto pageCount = this->pageCount();
    ASSERT(pageCount);

    switch (displayMode()) {
    case PDFDocumentLayout::DisplayMode::SinglePageDiscrete:
    case PDFDocumentLayout::DisplayMode::TwoUpDiscrete: {
        using PairedPagesLayoutBounds = std::pair<FloatRect, std::optional<FloatRect>>;

        auto layoutBoundsForRow = [this](PDFLayoutRow row) -> PairedPagesLayoutBounds {
            if (row.numPages() == 2)
                return { layoutBoundsForPageAtIndex(row.pages[0]), layoutBoundsForPageAtIndex(row.pages[1]) };

            return { layoutBoundsForPageAtIndex(row.pages[0]), { } };
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

        ASSERT(visibleRow);
        auto [leftPageLayoutBounds, rightPageLayoutBounds] = layoutBoundsForRow(*visibleRow);

        if (documentSpacePoint.y() < leftPageLayoutBounds.maxY() || (rightPageLayoutBounds && documentSpacePoint.y() < rightPageLayoutBounds->maxY())) {
            if (!rightPageLayoutBounds || leftPageLayoutBounds.contains(documentSpacePoint))
                return visibleRow->pages[0];

            if (rightPageLayoutBounds->contains(documentSpacePoint))
                return visibleRow->pages[1];

            if (minimumDistanceToPage(leftPageLayoutBounds) < minimumDistanceToPage(rightPageLayoutBounds.value()))
                return visibleRow->pages[0];

            if (visibleRow->numPages() == 2)
                return visibleRow->pages[1];
        }

        // The point is below the pages in the row.
        if (minimumDistanceToPage(leftPageLayoutBounds) < minimumDistanceToPage(rightPageLayoutBounds.value_or(FloatRect { })))
            return visibleRow->pages[0];

        if (visibleRow->numPages() == 2)
            return visibleRow->pages[1];

        return visibleRow->pages[0];
    }

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

            // FIXME: <https://webkit.org/b/276981> Share some code.
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
            if (visibleRow && !visibleRow->containsPage(index))
                continue;

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

    case PDFDocumentLayout::DisplayMode::SinglePageContinuous: {
        for (PDFDocumentLayout::PageIndex index = 0; index < pageCount; ++index) {
            auto pageBounds = layoutBoundsForPageAtIndex(index);

            if (documentSpacePoint.y() <= pageBounds.maxY())
                return index;
        }
        return lastPageIndex();
    }
    }
    ASSERT_NOT_REACHED();
    return pageCount - 1;
}

auto PDFDocumentLayout::pageIndexAndPagePointForDocumentYOffset(float documentYOffset) const -> std::pair<PageIndex, WebCore::FloatPoint>
{
    auto pageCount = this->pageCount();

    auto resultWithPageHorizontalCenterPoint = [&](PageIndex pageIndex, float documentYOffset) {
        auto pageBounds = layoutBoundsForPageAtIndex(pageIndex);
        auto pagePoint = documentToPDFPage(FloatPoint { pageBounds.center().x(), documentYOffset }, pageIndex);
        return std::make_pair(pageIndex, pagePoint);
    };

    switch (displayMode()) {
    case DisplayMode::TwoUpDiscrete:
    case DisplayMode::TwoUpContinuous:
        for (PageIndex index = 0; index < pageCount; ++index) {
            if (index == pageCount - 1)
                return resultWithPageHorizontalCenterPoint(index, documentYOffset);

            if (isRightPageIndex(index))
                continue;

            // Handle side by side pages with different sizes.
            std::optional<PageIndex> targetPageIndex = [&](PageIndex index) -> std::optional<PageIndex> {
                auto leftPageBounds = layoutBoundsForPageAtIndex(index);
                leftPageBounds.inflate(PDFDocumentLayout::documentMargin);
                if (documentYOffset >= leftPageBounds.y() && documentYOffset < leftPageBounds.maxY())
                    return index;

                auto rightPageIndex = index + 1;
                if (rightPageIndex == pageCount)
                    return { };

                auto rightPageBounds = layoutBoundsForPageAtIndex(rightPageIndex);
                rightPageBounds.inflate(PDFDocumentLayout::documentMargin);
                if (documentYOffset >= rightPageBounds.y() && documentYOffset < rightPageBounds.maxY())
                    return rightPageIndex;

                return { };
            }(index);

            if (!targetPageIndex)
                continue;

            return resultWithPageHorizontalCenterPoint(*targetPageIndex, documentYOffset);
        }
        break;
    case DisplayMode::SinglePageDiscrete:
    case DisplayMode::SinglePageContinuous: {
        for (PageIndex index = 0; index < pageCount; ++index) {
            auto pageBounds = layoutBoundsForPageAtIndex(index);
            if (documentYOffset <= pageBounds.maxY() || index == pageCount - 1)
                return resultWithPageHorizontalCenterPoint(index, documentYOffset);
        }
    }
    }
    ASSERT_NOT_REACHED();
    return { pageCount - 1, { } };
}

auto PDFDocumentLayout::updateLayout(IntSize pluginSize, ShouldUpdateAutoSizeScale shouldUpdateScale) -> OptionSet<LayoutUpdateChange>
{
    OptionSet<LayoutUpdateChange> layoutUpdateChanges;

    auto pageCount = this->pageCount();
    auto oldPageGeometry = std::exchange(m_pageGeometry, { });
    auto oldDocumentBounds = std::exchange(m_documentBounds, { });

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

    FloatSize maxRowSize;
    FloatSize currentRowSize;
    bool isTwoUpLayout = isTwoUpDisplayMode();

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
            if (isRightPageIndex(i)) {
                currentRowSize.expand(pageMargin.width() + pageBounds.width(), 0);
                currentRowSize = currentRowSize.expandedTo({ 0, pageBounds.height() });
                maxRowSize = maxRowSize.expandedTo(currentRowSize);
            } else {
                currentRowSize = pageBounds.size();
                if (i == pageCount - 1)
                    maxRowSize = maxRowSize.expandedTo(currentRowSize);
            }
        } else
            maxRowSize = maxRowSize.expandedTo(pageBounds.size());

        m_pageGeometry.append({ pageCropBox, pageBounds, rotation });
    }

    layoutUpdateChanges.set(LayoutUpdateChange::PageGeometries, oldPageGeometry != m_pageGeometry);

    layoutPages(pluginSize, maxRowSize, shouldUpdateScale);

    layoutUpdateChanges.set(LayoutUpdateChange::DocumentBounds, oldDocumentBounds != m_documentBounds);

    LOG_WITH_STREAM(PDF, stream << "PDFDocumentLayout::updateLayout() - plugin size " << pluginSize << " document bounds " << m_documentBounds << " scale " << m_scale);

    return layoutUpdateChanges;
}

void PDFDocumentLayout::layoutPages(FloatSize availableSize, FloatSize maxRowSize, ShouldUpdateAutoSizeScale shouldUpdateScale)
{
    // We always lay out in a continuous mode. We handle non-continuous mode via scroll snap.
    switch (m_displayMode) {
    case DisplayMode::SinglePageDiscrete:
    case DisplayMode::TwoUpDiscrete:
        layoutDiscreteRows(availableSize, maxRowSize, CenterRowVertically::Yes);
        break;

    case DisplayMode::SinglePageContinuous:
    case DisplayMode::TwoUpContinuous:
        layoutContinuousRows(availableSize, maxRowSize, CenterRowVertically::No);
        break;
    }

    if (shouldUpdateScale == ShouldUpdateAutoSizeScale::Yes) {
        auto contentWidth = maxRowSize.width() + 2 * documentMargin.width();
        m_scale = std::max<float>(availableSize.width() / contentWidth, minScale);
    }

    LOG_WITH_STREAM(PDF, stream << "PDFDocumentLayout::layoutContinuousRows - document bounds " << m_documentBounds << " scale " << m_scale);
}

void PDFDocumentLayout::layoutContinuousRows(FloatSize availableSize, FloatSize maxRowSize, CenterRowVertically centerVertically)
{
    float currentYOffset = documentMargin.height();

    for (auto& row : rows()) {
        auto rowBounds = layoutRow(row, maxRowSize, currentYOffset, centerVertically);
        currentYOffset = rowBounds.maxY();
    }

    currentYOffset -= pageMargin.height();
    currentYOffset += documentMargin.height();

    maxRowSize.expand(2 * documentMargin.width(), 0);
    m_documentBounds = FloatRect { 0, 0, maxRowSize.width(), currentYOffset };
}

void PDFDocumentLayout::layoutDiscreteRows(FloatSize availableSize, FloatSize maxRowSize, CenterRowVertically centerVertically)
{
    for (auto& row : rows())
        layoutRow(row, maxRowSize, documentMargin.height(), centerVertically);

    maxRowSize += 2 * documentMargin;
    m_documentBounds = FloatRect { { }, maxRowSize };
}

FloatRect PDFDocumentLayout::layoutRow(const PDFLayoutRow& row, FloatSize maxRowSize, float rowTop, CenterRowVertically centerVertically)
{
    FloatRect rowBounds;

    switch (row.numPages()) {
    case 1: {
        auto pageIndex = row.pages[0];
        auto pageBounds = m_pageGeometry[pageIndex].layoutBounds;
        auto pageLeft = documentMargin.width() + std::max<float>(std::floor((maxRowSize.width() - pageBounds.width()) / 2), 0);
        auto pageTop = rowTop;

        if (centerVertically == CenterRowVertically::Yes)
            pageTop += std::floor((maxRowSize.height() - pageBounds.height()) / 2);

        pageBounds.setLocation({ pageLeft, pageTop });
        rowBounds = pageBounds;

        LOG_WITH_STREAM(PDF, stream << "PDFDocumentLayout::layoutSingleColumn - page " << pageIndex << " maxRowSize " << maxRowSize << ", bounds " << pageBounds);

        m_pageGeometry[pageIndex].layoutBounds = pageBounds;
        break;
    }
    case 2: {
        auto leftPageIndex = row.pages[0];
        auto rightPageIndex = row.pages[1];

        auto leftPageBounds = m_pageGeometry[leftPageIndex].layoutBounds;
        auto rightPageBounds = m_pageGeometry[rightPageIndex].layoutBounds;

        // Center the pair of pages horizontally.
        float horizontalSpace = maxRowSize.width() - leftPageBounds.width() - rightPageBounds.width() - pageMargin.width();
        leftPageBounds.setX(std::floor(documentMargin.width() + horizontalSpace / 2));
        rightPageBounds.setX(leftPageBounds.maxX() + pageMargin.width());

        // Center each page vertically in the row.
        auto maxPageHeight = std::max(leftPageBounds.height(), rightPageBounds.height());

        float leftVerticalSpace = maxPageHeight - leftPageBounds.height();
        leftPageBounds.setY(rowTop + std::floor(leftVerticalSpace / 2));

        float rightVerticalSpace = maxPageHeight - rightPageBounds.height();
        rightPageBounds.setY(rowTop + std::floor(rightVerticalSpace / 2));

        m_pageGeometry[leftPageIndex].layoutBounds = leftPageBounds;
        m_pageGeometry[rightPageIndex].layoutBounds = rightPageBounds;

        LOG_WITH_STREAM(PDF, stream << "PDFDocumentLayout::layoutTwoUpColumn - left page bounds " << leftPageBounds << " right page bounds " << rightPageBounds);

        rowBounds = unionRect(leftPageBounds, rightPageBounds);
        break;
    }
    }

    rowBounds.inflateY(pageMargin.height());
    return rowBounds;
}

size_t PDFDocumentLayout::pageCount() const
{
    if (!m_pdfDocument)
        return 0;

    return [m_pdfDocument pageCount];
}

size_t PDFDocumentLayout::rowCount() const
{
    if (!m_pdfDocument)
        return 0;

    if (isTwoUpDisplayMode())
        return std::round(pageCount() / 2);

    return pageCount();
}

Vector<PDFLayoutRow> PDFDocumentLayout::rows() const
{
    if (!m_pdfDocument)
        return { };

    Vector<PDFLayoutRow> rows;
    rows.reserveInitialCapacity(rowCount());

    auto pageCount = this->pageCount();

    if (isSinglePageDisplayMode()) {
        for (PageIndex i = 0; i < pageCount; ++i)
            rows.append(PDFLayoutRow { { i } });

        return rows;
    }

    // Two-up mode.
    auto numFullRows = pageCount / 2;
    for (unsigned rowIndex = 0; rowIndex < numFullRows; ++rowIndex) {
        PageIndex leftPageIndex = rowIndex * 2;
        rows.append(PDFLayoutRow { { leftPageIndex, leftPageIndex + 1 } });
    }

    if (pageCount % 2)
        rows.append(PDFLayoutRow { { lastPageIndex() } });

    return rows;
}

PDFLayoutRow PDFDocumentLayout::rowForPageIndex(PageIndex index) const
{
    if (!m_pdfDocument)
        return { };

    if (isTwoUpDisplayMode()) {
        if (isLeftPageIndex(index) && index < lastPageIndex())
            return { { index, index + 1 } };

        if (isRightPageIndex(index)) {
            ASSERT(index);
            return { { index - 1, index } };
        }
    }

    return { { index } };
}

unsigned PDFDocumentLayout::rowIndexForPageIndex(PageIndex index) const
{
    if (isSinglePageDisplayMode())
        return index;

    return index / 2;
}

FloatRect PDFDocumentLayout::layoutBoundsForPageAtIndex(PageIndex index) const
{
    if (index >= m_pageGeometry.size())
        return { };

    return m_pageGeometry[index].layoutBounds;
}

FloatRect PDFDocumentLayout::layoutBoundsForRow(PDFLayoutRow layoutRow) const
{
    auto bounds = layoutBoundsForPageAtIndex(layoutRow.pages[0]);
    if (layoutRow.numPages() == 2)
        bounds.unite(layoutBoundsForPageAtIndex(layoutRow.pages[1]));

    bounds.inflate(PDFDocumentLayout::documentMargin);
    return bounds;
}

IntDegrees PDFDocumentLayout::rotationForPageAtIndex(PageIndex index) const
{
    if (index >= m_pageGeometry.size())
        return 0;

    return m_pageGeometry[index].rotation;
}

FloatSize PDFDocumentLayout::contentsSize() const
{
    return m_documentBounds.size();
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
