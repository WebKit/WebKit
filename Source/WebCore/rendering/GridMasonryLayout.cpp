/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "GridMasonryLayout.h"

#include "GridLayoutFunctions.h"
#include "RenderBoxInlines.h"
#include "RenderGrid.h"
#include "RenderStyleInlines.h"
#include "StyleGridPositionsResolver.h"
#include "StyleItemTolerance.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "WritingMode.h"

namespace WebCore {

void GridMasonryLayout::initializeMasonry(unsigned gridAxisTracks, Style::GridTrackSizingDirection masonryAxisDirection)
{
    // Reset global variables as they may contain state from previous runs of Masonry.
    m_masonryAxisDirection = masonryAxisDirection;
    m_masonryAxisGridGap = m_renderGrid->gridGap(m_masonryAxisDirection);
    m_gridAxisTracksCount = gridAxisTracks;
    m_gridContentSize = 0;

    m_renderGrid->currentGrid().setupGridForMasonryLayout();
    m_renderGrid->populateExplicitGridAndOrderIterator();

    resizeAndResetRunningPositions();
}

void GridMasonryLayout::performMasonryPlacement(const GridTrackSizingAlgorithm& algorithm, unsigned gridAxisTracks, Style::GridTrackSizingDirection masonryAxisDirection, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    initializeMasonry(gridAxisTracks, masonryAxisDirection);

    m_renderGrid->populateGridPositionsForDirection(algorithm, Style::GridTrackSizingDirection::Columns);
    m_renderGrid->populateGridPositionsForDirection(algorithm, Style::GridTrackSizingDirection::Rows);

    // 2.3 Masonry Layout Algorithm
    // https://drafts.csswg.org/css-grid-3/#masonry-layout-algorithm
    
    // the insertIntoGridAndLayoutItem() will modify the m_autoFlowNextCursor, so m_autoFlowNextCursor needs to be reset.
    m_autoFlowNextCursor = 0;

    placeMasonryItems(algorithm, layoutPhase);
}

void GridMasonryLayout::resizeAndResetRunningPositions()
{
    m_runningPositions.fill(LayoutUnit(), m_gridAxisTracksCount);
}

void GridMasonryLayout::placeMasonryItems(const GridTrackSizingAlgorithm& algorithm, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    if (!m_gridAxisTracksCount)
        return;

    auto& grid = m_renderGrid->currentGrid();
    for (CheckedPtr gridItem = grid.orderIterator().first(); gridItem; gridItem = grid.orderIterator().next()) {
        if (grid.orderIterator().shouldSkipChild(*gridItem))
            continue;

        auto gridArea = hasDefiniteGridAxisPosition(*gridItem, gridAxisDirection()) ? gridAreaForDefiniteGridAxisItem(*gridItem) : gridAreaForIndefiniteGridAxisItem(*gridItem);
        insertIntoGridAndLayoutItem(algorithm, *gridItem, gridArea, layoutPhase);
    }
}

GridArea GridMasonryLayout::gridAreaForDefiniteGridAxisItem(const RenderBox& gridItem) const
{
    auto itemSpan = m_renderGrid->currentGrid().gridItemSpan(gridItem, gridAxisDirection());
    ASSERT(!itemSpan.isIndefinite());
    itemSpan.translate(m_renderGrid->currentGrid().explicitGridStart(gridAxisDirection()));
    return masonryGridAreaFromGridAxisSpan(itemSpan);
}

LayoutUnit GridMasonryLayout::calculateMasonryIntrinsicLogicalWidth(RenderBox& gridItem, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    switch (layoutPhase) {
    case MasonryLayoutPhase::MinContentPhase:
        return gridItem.computeIntrinsicLogicalWidthUsing(CSS::Keyword::MinContent { }, { }, gridItem.borderAndPaddingLogicalWidth());
    case MasonryLayoutPhase::MaxContentPhase:
        return gridItem.computeIntrinsicLogicalWidthUsing(CSS::Keyword::MaxContent { }, { }, gridItem.borderAndPaddingLogicalWidth());
    case MasonryLayoutPhase::LayoutPhase:
        ASSERT_NOT_REACHED();
        return { };
    }

    return { };
}

void GridMasonryLayout::setItemGridAxisContainingBlockToGridArea(const GridTrackSizingAlgorithm& algorithm, RenderBox& gridItem)
{
    if (auto direction = gridAxisDirection(); direction == Style::GridTrackSizingDirection::Columns)
        gridItem.setGridAreaContentLogicalWidth(algorithm.gridAreaBreadthForGridItem(gridItem, direction));
    else
        gridItem.setGridAreaContentLogicalHeight(algorithm.gridAreaBreadthForGridItem(gridItem, direction));
    
    // FIXME(249230): Try to cache masonry layout sizes
    gridItem.setChildNeedsLayout(MarkOnlyThis);
}

void GridMasonryLayout::insertIntoGridAndLayoutItem(const GridTrackSizingAlgorithm& algorithm, RenderBox& gridItem, const GridArea& area, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    auto shouldOverrideLogicalWidth = [&](RenderBox& gridItem, GridMasonryLayout::MasonryLayoutPhase layoutPhase) {
        if (layoutPhase == MasonryLayoutPhase::LayoutPhase)
            return false;

        if (!(gridItem.style().logicalWidth().isAuto() || gridItem.style().logicalWidth().isPercent()))
            return false;

        ASSERT(m_renderGrid->isMasonry(Style::GridTrackSizingDirection::Columns));

        if (gridItem.style().writingMode().isOrthogonal(m_renderGrid->style().writingMode()))
            return false;

        if (auto* renderGrid = dynamicDowncast<RenderGrid>(gridItem); renderGrid && renderGrid->isSubgridRows())
            return false;

        return true;
    };

    if (shouldOverrideLogicalWidth(gridItem, layoutPhase))
        gridItem.setOverridingBorderBoxLogicalWidth(calculateMasonryIntrinsicLogicalWidth(gridItem, layoutPhase));

    m_renderGrid->currentGrid().insert(gridItem, area);
    setItemGridAxisContainingBlockToGridArea(algorithm, gridItem);
    gridItem.layoutIfNeeded();
    updateRunningPositions(gridItem, area);
    m_autoFlowNextCursor = gridAxisSpanFromArea(area).endLine() % m_gridAxisTracksCount;
}

LayoutUnit GridMasonryLayout::masonryAxisMarginBoxForItem(const RenderBox& gridItem)
{
    LayoutUnit marginBoxSize;
    if (m_masonryAxisDirection == Style::GridTrackSizingDirection::Rows) {
        if (GridLayoutFunctions::isOrthogonalGridItem(m_renderGrid, gridItem))
            marginBoxSize = gridItem.isHorizontalWritingMode() ? gridItem.width() + gridItem.horizontalMarginExtent() : gridItem.height() + gridItem.verticalMarginExtent();
        else
            marginBoxSize = gridItem.logicalHeight() + gridItem.marginLogicalHeight();

    } else {
        if (GridLayoutFunctions::isOrthogonalGridItem(m_renderGrid, gridItem))
            marginBoxSize = gridItem.isHorizontalWritingMode() ? gridItem.height() + gridItem.verticalMarginExtent() : gridItem.width() + gridItem.horizontalMarginExtent();
        else
            marginBoxSize = gridItem.logicalWidth() + gridItem.marginLogicalWidth();
    }
    return marginBoxSize;
}

void GridMasonryLayout::updateRunningPositions(const RenderBox& gridItem, const GridArea& area)
{
    auto gridAxisSpan = gridAxisSpanFromArea(area);
    ASSERT(gridAxisSpan.startLine() < m_runningPositions.size() && gridAxisSpan.endLine() <= m_runningPositions.size());
    gridAxisSpan.clamp(m_runningPositions.size());

    LayoutUnit previousRunningPosition;
    for (auto line : gridAxisSpan)
        previousRunningPosition = std::max(previousRunningPosition, m_runningPositions[line]);

    auto newRunningPosition = masonryAxisMarginBoxForItem(gridItem) + previousRunningPosition + m_masonryAxisGridGap;
    m_gridContentSize = std::max(m_gridContentSize, newRunningPosition - m_masonryAxisGridGap);

    for (auto span : gridAxisSpan)
        m_runningPositions[span] = std::max(m_runningPositions[span], newRunningPosition);

    updateItemOffset(gridItem, previousRunningPosition);
}

void GridMasonryLayout::updateItemOffset(const RenderBox& gridItem, LayoutUnit offset)
{
    // We set() and not add() to update the value if the |gridItem| is already inserted
    m_itemOffsets.set(gridItem, offset);
}

LayoutUnit GridMasonryLayout::maxRunningPositionForSpan(unsigned startLine, unsigned spanLength) const
{
    LayoutUnit maxPosition;
    for (unsigned lineOffset = 0; lineOffset < spanLength; lineOffset++)
        maxPosition = std::max(maxPosition, m_runningPositions[startLine + lineOffset]);
    return maxPosition;
}

GridArea GridMasonryLayout::gridAreaForIndefiniteGridAxisItem(const RenderBox& item)
{
    auto itemSpanLength = Style::GridPositionsResolver::spanSizeForAutoPlacedItem(item, gridAxisDirection());
    auto gridAxisLines = m_gridAxisTracksCount + 1;

    // Get item-tolerance from the masonry container's style
    const auto& tolerance = m_renderGrid->style().itemTolerance();

    if (tolerance.isInfinite()) {
        // Infinite tolerance: place items strictly in order without considering track lengths
        // Use round-robin placement starting from the cursor position
        auto startingLine = m_autoFlowNextCursor;

        // If the item doesn't fit at the cursor position, wrap to the beginning
        if (startingLine + itemSpanLength > m_gridAxisTracksCount)
            startingLine = 0;

        auto gridAxisPosition = GridSpan::translatedDefiniteGridSpan(startingLine, startingLine + itemSpanLength);
        return masonryGridAreaFromGridAxisSpan(gridAxisPosition);
    }

    // For normal and length-percentage tolerances, find positions within tolerance of the shortest track
    // Calculate tolerance value
    auto contentBoxSize = gridAxisDirection() == Style::GridTrackSizingDirection::Columns
        ? m_renderGrid->contentBoxLogicalHeight()
        : m_renderGrid->contentBoxLogicalWidth();

    LayoutUnit toleranceValue = tolerance.switchOn(
        [&](const CSS::Keyword::Normal&) -> LayoutUnit {
            // Normal resolves to 1em
            return LayoutUnit { m_renderGrid->checkedStyle()->computedFontSize() };
        },
        [&](const typename Style::ItemTolerance::Fixed& fixed) -> LayoutUnit {
            return LayoutUnit { fixed.resolveZoom(m_renderGrid->style().usedZoomForLength()) };
        },
        [&](const typename Style::ItemTolerance::Percentage& percentage) -> LayoutUnit {
            return Style::evaluate<LayoutUnit>(percentage, contentBoxSize);
        },
        [&](const typename Style::ItemTolerance::Calc& calc) -> LayoutUnit {
            return Style::evaluate<LayoutUnit>(calc, contentBoxSize, m_renderGrid->style().usedZoomForLength());
        },
        [](const CSS::Keyword::Infinite&) -> LayoutUnit {
            // This case shouldn't be reached due to the outer if check
            ASSERT_NOT_REACHED();
            return LayoutUnit { };
        }
    );

    // Step 1: Find the absolute shortest position across all tracks
    auto maxStartingLine = gridAxisLines - itemSpanLength;
    LayoutUnit absoluteShortest = LayoutUnit::max();
    for (unsigned i = 0; i < maxStartingLine; i++)
        absoluteShortest = std::min(absoluteShortest, maxRunningPositionForSpan(i, itemSpanLength));

    // Step 2: Find first position within tolerance of shortest
    unsigned smallestMaxPosLine = 0;
    for (unsigned i = 0; i < maxStartingLine; i++) {
        // Start from cursor position and wrap around
        auto startingLine = (m_autoFlowNextCursor + i) % maxStartingLine;

        auto maxPosForCurrentStartingLine = maxRunningPositionForSpan(startingLine, itemSpanLength);

        // Accept first position within tolerance of the absolute shortest
        if (maxPosForCurrentStartingLine <= absoluteShortest + toleranceValue) {
            smallestMaxPosLine = startingLine;
            break;
        }
    }

    auto gridAxisPosition = GridSpan::translatedDefiniteGridSpan(smallestMaxPosLine, smallestMaxPosLine + itemSpanLength);
    return masonryGridAreaFromGridAxisSpan(gridAxisPosition);
}

LayoutUnit GridMasonryLayout::offsetForGridItem(const RenderBox& gridItem) const
{
    const auto& offsetIter = m_itemOffsets.find(gridItem);
    if (offsetIter == m_itemOffsets.end())
        return 0_lu;
    return offsetIter->value;
}

inline Style::GridTrackSizingDirection GridMasonryLayout::gridAxisDirection() const
{
    // The masonry axis and grid axis can never be the same. 
    // They are always perpendicular to each other.
    return orthogonalDirection(m_masonryAxisDirection);
}

bool GridMasonryLayout::hasDefiniteGridAxisPosition(const RenderBox& gridItem, Style::GridTrackSizingDirection gridAxisDirection) const
{
    return !Style::GridPositionsResolver::resolveGridPositionsFromStyle(m_renderGrid, gridItem, gridAxisDirection).isIndefinite();
}

GridSpan GridMasonryLayout::gridAxisSpanFromArea(const GridArea& gridArea) const
{
    return gridArea.span(gridAxisDirection());
}

GridArea GridMasonryLayout::masonryGridAreaFromGridAxisSpan(const GridSpan& gridAxisSpan) const
{
    return m_masonryAxisDirection == Style::GridTrackSizingDirection::Rows
        ? GridArea { m_masonryAxisSpan, gridAxisSpan }
        : GridArea { gridAxisSpan, m_masonryAxisSpan };
}
} // end namespace WebCore
