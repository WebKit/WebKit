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
#include "GridLanesLayout.h"

#include "GridLayoutFunctions.h"
#include "RenderBoxInlines.h"
#include "RenderGrid.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleGridPositionsResolver.h"
#include "WritingMode.h"

namespace WebCore {

GridLanesLayout::GridLanesLayout(RenderGrid& renderGrid, unsigned gridAxisTracksCount, Style::GridTrackSizingDirection stackingAxisDirection)
    : m_runningPositions(gridAxisTracksCount)
    , m_renderGrid(renderGrid)
    , m_stackingAxisGridGap(renderGrid.gridGap(stackingAxisDirection))
    , m_stackingAxisDirection(stackingAxisDirection)
{
    m_renderGrid->currentGrid().setupForGridLanesLayout();
    m_renderGrid->populateExplicitGridAndOrderIterator();
}

void GridLanesLayout::performGridLanesPlacement(const GridTrackSizingAlgorithm& algorithm, ResolvedFitTolerance fitTolerance, Phase layoutPhase)
{
    // 4.4 Grid Lanes Layout and Placement Algorithm
    // https://drafts.csswg.org/css-grid-3/#grid-lanes-layout-algorithm
    placeGridLanesItems(algorithm, fitTolerance, layoutPhase);
}

void GridLanesLayout::placeGridLanesItems(const GridTrackSizingAlgorithm& algorithm, ResolvedFitTolerance fitTolerance, Phase layoutPhase)
{
    if (!gridAxisTracksCount())
        return;

    auto& grid = m_renderGrid->currentGrid();
    for (CheckedPtr gridItem = grid.orderIterator().first(); gridItem; gridItem = grid.orderIterator().next()) {
        if (grid.orderIterator().shouldSkipChild(*gridItem))
            continue;

        bool isAutoPlacedInGridAxis = !hasDefiniteGridAxisPosition(*gridItem, gridAxisDirection());
        auto gridArea = isAutoPlacedInGridAxis ? gridAreaForIndefiniteGridAxisItem(*gridItem, fitTolerance) : gridAreaForDefiniteGridAxisItem(*gridItem);
        insertIntoGridAndLayoutItem(algorithm, *gridItem, gridArea, layoutPhase);

        if (isAutoPlacedInGridAxis)
            m_autoFlowNextCursor = gridAxisSpanFromArea(gridArea).endLine() % gridAxisTracksCount();
    }
}

GridArea GridLanesLayout::gridAreaForDefiniteGridAxisItem(const RenderBox& gridItem) const
{
    auto itemSpan = m_renderGrid->currentGrid().gridItemSpan(gridItem, gridAxisDirection());
    ASSERT(!itemSpan.isIndefinite());
    itemSpan.translate(m_renderGrid->currentGrid().explicitGridStart(gridAxisDirection()));
    return gridAreaFromGridAxisSpan(itemSpan);
}

LayoutUnit GridLanesLayout::calculateGridLanesIntrinsicLogicalWidth(RenderBox& gridItem, Phase layoutPhase)
{
    switch (layoutPhase) {
    case Phase::MinContent:
        return gridItem.computeSizingKeywordLogicalWidthUsing(CSS::Keyword::MinContent { }, { }, gridItem.borderAndPaddingLogicalWidth());
    case Phase::MaxContent:
        return gridItem.computeSizingKeywordLogicalWidthUsing(CSS::Keyword::MaxContent { }, { }, gridItem.borderAndPaddingLogicalWidth());
    case Phase::Layout:
        ASSERT_NOT_REACHED();
        return { };
    }

    return { };
}

void GridLanesLayout::setItemContainingBlockToGridArea(const GridTrackSizingAlgorithm& algorithm, RenderBox& gridItem)
{
    CheckedPtr<RenderGrid> containingBlock = dynamicDowncast<RenderGrid>(gridItem.containingBlock());
    if (!containingBlock) {
        ASSERT_NOT_REACHED();
        return;
    }

    // FIXME: We need to set both axes here because RenderGrid sets and expects them all over the place.
    // Ideally we untangle all that and only set the grid axis that we need. webkit.org/b/305136
    auto direction = gridAxisDirection();
    if (direction == Style::GridTrackSizingDirection::Columns) {
        gridItem.setGridAreaContentLogicalWidth(algorithm.gridAreaBreadthForGridItem(gridItem, direction));
        gridItem.setGridAreaContentLogicalHeight(containingBlock->availableLogicalHeightForContentBox());
    } else {
        gridItem.setGridAreaContentLogicalHeight(algorithm.gridAreaBreadthForGridItem(gridItem, direction));
        gridItem.setGridAreaContentLogicalWidth(containingBlock->contentBoxLogicalWidth());
    }

    // FIXME(249230): Try to cache grid lanes layout sizes
    gridItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
}

void GridLanesLayout::insertIntoGridAndLayoutItem(const GridTrackSizingAlgorithm& algorithm, RenderBox& gridItem, const GridArea& area, Phase layoutPhase)
{
    auto shouldOverrideLogicalWidth = [&](RenderBox& gridItem, Phase layoutPhase) {
        if (layoutPhase == Phase::Layout)
            return false;

        if (!(gridItem.style().logicalWidth().isAuto() || gridItem.style().logicalWidth().isPercent()))
            return false;

        ASSERT(m_renderGrid->isStackingAxis(Style::GridTrackSizingDirection::Columns));

        if (gridItem.style().writingMode().isOrthogonal(m_renderGrid->style().writingMode()))
            return false;

        if (auto* renderGrid = dynamicDowncast<RenderGrid>(gridItem); renderGrid && renderGrid->isSubgridRows())
            return false;

        return true;
    };

    if (shouldOverrideLogicalWidth(gridItem, layoutPhase))
        gridItem.setOverridingBorderBoxLogicalWidth(calculateGridLanesIntrinsicLogicalWidth(gridItem, layoutPhase));

    m_renderGrid->currentGrid().insert(gridItem, area);
    setItemContainingBlockToGridArea(algorithm, gridItem);
    gridItem.layoutIfNeeded();
    updateRunningPositions(gridItem, area);
}

LayoutUnit GridLanesLayout::stackingAxisMarginBoxForItem(const RenderBox& gridItem)
{
    LayoutUnit marginBoxSize;
    if (m_stackingAxisDirection == Style::GridTrackSizingDirection::Rows) {
        if (GridLayoutFunctions::isOrthogonalGridItem(m_renderGrid, gridItem))
            marginBoxSize = gridItem.isHorizontalWritingMode() ? gridItem.borderBoxWidth() + gridItem.horizontalMarginExtent() : gridItem.borderBoxHeight() + gridItem.verticalMarginExtent();
        else
            marginBoxSize = gridItem.logicalHeight() + gridItem.marginLogicalHeight();

    } else {
        if (GridLayoutFunctions::isOrthogonalGridItem(m_renderGrid, gridItem))
            marginBoxSize = gridItem.isHorizontalWritingMode() ? gridItem.borderBoxHeight() + gridItem.verticalMarginExtent() : gridItem.borderBoxWidth() + gridItem.horizontalMarginExtent();
        else
            marginBoxSize = gridItem.logicalWidth() + gridItem.marginLogicalWidth();
    }
    return marginBoxSize;
}

void GridLanesLayout::updateRunningPositions(const RenderBox& gridItem, const GridArea& area)
{
    auto gridAxisSpan = gridAxisSpanFromArea(area);
    ASSERT(gridAxisSpan.startLine() < m_runningPositions.size() && gridAxisSpan.endLine() <= m_runningPositions.size());
    gridAxisSpan.clamp(m_runningPositions.size());

    LayoutUnit previousRunningPosition;
    for (auto line : gridAxisSpan)
        previousRunningPosition = std::max(previousRunningPosition, m_runningPositions[line]);

    auto newRunningPosition = stackingAxisMarginBoxForItem(gridItem) + previousRunningPosition + m_stackingAxisGridGap;
    m_gridContentSize = std::max(m_gridContentSize, newRunningPosition - m_stackingAxisGridGap);

    for (auto span : gridAxisSpan)
        m_runningPositions[span] = newRunningPosition;

    updateItemOffset(gridItem, previousRunningPosition);
}

void GridLanesLayout::updateItemOffset(const RenderBox& gridItem, LayoutUnit offset)
{
    // We set() and not add() to update the value if the |gridItem| is already inserted
    m_itemOffsets.set(gridItem, offset);
}

LayoutUnit GridLanesLayout::maxRunningPositionForSpan(unsigned startLine, unsigned spanLength) const
{
    LayoutUnit maxPosition;
    for (unsigned lineOffset = 0; lineOffset < spanLength; lineOffset++)
        maxPosition = std::max(maxPosition, m_runningPositions[startLine + lineOffset]);
    return maxPosition;
}

GridArea GridLanesLayout::gridAreaForIndefiniteGridAxisItem(const RenderBox& item, ResolvedFitTolerance fitTolerance)
{
    auto itemSpanLength = std::min<unsigned>(Style::GridPositionsResolver::spanSizeForAutoPlacedItem(item, gridAxisDirection()), gridAxisTracksCount());
    auto gridAxisLines = gridAxisTracksCount() + 1;

    if (WTF::holdsAlternative<CSS::Keyword::Infinite>(fitTolerance)) {
        // Infinite tolerance: place items strictly in order without considering track lengths
        // Use round-robin placement starting from the cursor position
        auto startingLine = m_autoFlowNextCursor;

        // If the item doesn't fit at the cursor position, wrap to the beginning
        if (startingLine + itemSpanLength > gridAxisTracksCount())
            startingLine = 0;

        auto gridAxisPosition = GridSpan::translatedDefiniteGridSpan(startingLine, startingLine + itemSpanLength);
        return gridAreaFromGridAxisSpan(gridAxisPosition);
    }

    // For normal and length-percentage tolerances, find positions within tolerance of the shortest track
    auto toleranceValue = std::get<LayoutUnit>(fitTolerance);

    // Step 1: Find the absolute shortest position across all tracks
    auto maxStartingLine = gridAxisLines - itemSpanLength;
    LayoutUnit absoluteShortest = LayoutUnit::max();
    for (unsigned i = 0; i < maxStartingLine; i++)
        absoluteShortest = std::min(absoluteShortest, maxRunningPositionForSpan(i, itemSpanLength));

    // Step 2: Find first position within tolerance of shortest, starting from the cursor position.
    unsigned smallestMaxPosLine = 0;
    auto autoFlowNextCursorShift = (m_autoFlowNextCursor > maxStartingLine) ? 0 : m_autoFlowNextCursor;
    for (unsigned i = 0; i < maxStartingLine; i++) {
        auto startingLine = (autoFlowNextCursorShift + i) % maxStartingLine;

        auto maxPosForCurrentStartingLine = maxRunningPositionForSpan(startingLine, itemSpanLength);

        // Accept first position within tolerance of the absolute shortest
        if (maxPosForCurrentStartingLine <= absoluteShortest + toleranceValue) {
            smallestMaxPosLine = startingLine;
            break;
        }
    }

    auto gridAxisPosition = GridSpan::translatedDefiniteGridSpan(smallestMaxPosLine, smallestMaxPosLine + itemSpanLength);
    return gridAreaFromGridAxisSpan(gridAxisPosition);
}

LayoutUnit GridLanesLayout::offsetForGridItem(const RenderBox& gridItem) const
{
    const auto& offsetIter = m_itemOffsets.find(gridItem);
    if (offsetIter == m_itemOffsets.end())
        return 0_lu;
    return offsetIter->value;
}

inline Style::GridTrackSizingDirection GridLanesLayout::gridAxisDirection() const
{
    // The stacking axis and grid axis can never be the same.
    // They are always perpendicular to each other.
    return orthogonalDirection(m_stackingAxisDirection);
}

bool GridLanesLayout::hasDefiniteGridAxisPosition(const RenderBox& gridItem, Style::GridTrackSizingDirection gridAxisDirection) const
{
    return !Style::GridPositionsResolver::resolveGridPositionsFromStyle(m_renderGrid, gridItem, gridAxisDirection).isIndefinite();
}

GridSpan GridLanesLayout::gridAxisSpanFromArea(const GridArea& gridArea) const
{
    return gridArea.span(gridAxisDirection());
}

GridArea GridLanesLayout::gridAreaFromGridAxisSpan(const GridSpan& gridAxisSpan) const
{
    auto stackingAxisSpan = GridSpan::stackingAxisTranslatedDefiniteGridSpan();
    return m_stackingAxisDirection == Style::GridTrackSizingDirection::Rows
        ? GridArea { stackingAxisSpan, gridAxisSpan }
        : GridArea { gridAxisSpan, stackingAxisSpan };
}
} // end namespace WebCore
