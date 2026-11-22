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
#include "RenderLayoutState.h"
#include "RenderObjectInlines.h"
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

GridLanesResult GridLanesLayout::performGridLanesPlacement(const GridTrackSizingAlgorithm& algorithm, ResolvedFitTolerance fitTolerance, Phase layoutPhase)
{
    // 4.4 Grid Lanes Layout and Placement Algorithm
    // https://drafts.csswg.org/css-grid-3/#grid-lanes-layout-algorithm
    return placeGridLanesItems(algorithm, fitTolerance, layoutPhase);
}

GridLanesResult GridLanesLayout::placeGridLanesItems(const GridTrackSizingAlgorithm& algorithm, ResolvedFitTolerance fitTolerance, Phase layoutPhase)
{
    if (!gridAxisTracksCount())
        return { };

    struct PlacedItem {
        CheckedRef<const RenderBox> gridItem;
        GridSpan gridAxisTracks;
        LayoutUnit marginBoxStart;
    };
    Vector<PlacedItem> placedItems;
    LayoutUnit gridContentSize;

    auto& grid = m_renderGrid->currentGrid();
    for (CheckedRef gridItem : grid.orderIterator().gridItems()) {
        bool isAutoPlacedInGridAxis = !hasDefiniteGridAxisPosition(gridItem, gridAxisDirection());
        auto gridArea = isAutoPlacedInGridAxis ? gridAreaForIndefiniteGridAxisItem(gridItem, fitTolerance) : gridAreaForDefiniteGridAxisItem(gridItem);
        layoutGridItem(algorithm, gridItem, gridArea, layoutPhase);
        auto placement = updateRunningPositions(gridArea, stackingAxisMarginBoxForItem(gridItem));

        if (isAutoPlacedInGridAxis)
            m_autoFlowNextCursor = gridAxisSpanFromArea(gridArea).endLine() % gridAxisTracksCount();
        grid.insert(gridItem, gridArea);

        auto gridAxisTracks = gridAxisSpanFromArea(gridArea);
        gridAxisTracks.clamp(gridAxisTracksCount());
        placedItems.append({ gridItem, gridAxisTracks, placement.marginBoxStart });
        gridContentSize = std::max(gridContentSize, placement.marginBoxEnd);
    }

    // "For the last item in a track, the alignment container extends to the lowest bottom outer edge among all the last items in all tracks."
    auto lowestLastItemEdge = std::ranges::max(m_runningPositions) - m_stackingAxisGridGap;
    Vector<LayoutUnit> alignmentContainerEnds(FillWith { }, gridAxisTracksCount(), lowestLastItemEdge);
    HashMap<SingleThreadWeakRef<const RenderBox>, GridLanesResult::StackingAxisAlignmentContainer> stackingAxisAlignmentContainers;
    // "In the stacking axis, the self-alignment properties only apply to items that are adjacent to a “gap” in the layout,
    // i.e. placed in their grid track(s) either immediately before a spanning item or as the last item."
    for (auto placedItem : std::views::reverse(placedItems)) {
        auto alignmentContainerEnd = lowestLastItemEdge;
        // Walking backwards, alignmentContainerEnds[track] is where the next item placed in the track starts, less the gap.
        for (auto track : placedItem.gridAxisTracks) {
            alignmentContainerEnd = std::min(alignmentContainerEnd, alignmentContainerEnds[track]);
            alignmentContainerEnds[track] = placedItem.marginBoxStart - m_stackingAxisGridGap;
        }
        stackingAxisAlignmentContainers.set(placedItem.gridItem.get(), GridLanesResult::StackingAxisAlignmentContainer { placedItem.marginBoxStart, alignmentContainerEnd - placedItem.marginBoxStart });
    }

    return { WTF::move(stackingAxisAlignmentContainers), gridContentSize };
}

GridArea GridLanesLayout::gridAreaForDefiniteGridAxisItem(const RenderBox& gridItem) const
{
    auto& grid = m_renderGrid->currentGrid();
    auto itemArea = grid.gridItemArea(gridItem);
    grid.clampAreaToSubgridIfNeeded(itemArea);
    auto gridAxisPosition = gridAxisSpanFromArea(itemArea);
    ASSERT(!gridAxisPosition.isIndefinite());
    gridAxisPosition.translate(grid.explicitGridStart(gridAxisDirection()));
    return gridAreaFromGridAxisSpan(gridAxisPosition);
}

LayoutUnit GridLanesLayout::calculateGridLanesIntrinsicLogicalWidth(RenderBox& gridItem, Phase layoutPhase)
{
    auto intrinsicWidthComputationScope = IntrinsicLogicalWidthComputationScope { protect(gridItem.layoutContext()), gridItem };
    switch (layoutPhase) {
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
    gridItem.setChildNeedsLayout(MarkingBehavior::OnlyThis);
}

void GridLanesLayout::layoutGridItem(const GridTrackSizingAlgorithm& algorithm, RenderBox& gridItem, const GridArea& area, Phase layoutPhase)
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

    // The item is inserted into the grid once its tracks are settled.
    m_renderGrid->currentGrid().setGridItemArea(gridItem, area);
    setItemContainingBlockToGridArea(algorithm, gridItem);
    gridItem.layoutIfNeeded();
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

GridLanesLayout::StackingAxisPlacement GridLanesLayout::updateRunningPositions(const GridArea& area, LayoutUnit marginBoxSize)
{
    auto gridAxisSpan = gridAxisSpanFromArea(area);
    ASSERT(gridAxisSpan.startLine() < m_runningPositions.size() && gridAxisSpan.endLine() <= m_runningPositions.size());
    gridAxisSpan.clamp(m_runningPositions.size());

    LayoutUnit previousRunningPosition;
    for (auto track : gridAxisSpan)
        previousRunningPosition = std::max(previousRunningPosition, m_runningPositions[track]);

    auto marginBoxEnd = previousRunningPosition + marginBoxSize;

    // "Set the running position of the spanned grid axis tracks to max_pos + outer size + grid-gap.
    // For this purpose, the outer sizes of the box are floored at zero."
    auto runningPosition = previousRunningPosition + std::max(0_lu, marginBoxSize) + m_stackingAxisGridGap;
    for (auto track : gridAxisSpan)
        m_runningPositions[track] = runningPosition;

    return { previousRunningPosition, marginBoxEnd };
}

// "A collapsed grid track cannot accept placement of auto-placed items."
bool GridLanesLayout::tracksAcceptAutoPlacedItem(unsigned startLine, unsigned trackCount) const
{
    return !m_renderGrid->currentGrid().containsEmptyAutoRepeatTrack(gridAxisDirection(), GridSpan::translatedDefiniteGridSpan(startLine, startLine + trackCount));
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

        for (unsigned attempt = 0; attempt < gridAxisTracksCount() && !tracksAcceptAutoPlacedItem(startingLine, itemSpanLength); ++attempt) {
            if (++startingLine + itemSpanLength > gridAxisTracksCount())
                startingLine = 0;
        }

        auto gridAxisPosition = GridSpan::translatedDefiniteGridSpan(startingLine, startingLine + itemSpanLength);
        return gridAreaFromGridAxisSpan(gridAxisPosition);
    }

    // For normal and length-percentage tolerances, find positions within tolerance of the shortest track
    auto toleranceValue = std::get<LayoutUnit>(fitTolerance);

    // Step 1: Find the absolute shortest position across all tracks
    auto maxStartingLine = gridAxisLines - itemSpanLength;
    LayoutUnit absoluteShortest = LayoutUnit::max();
    for (unsigned startingLine = 0; startingLine < maxStartingLine; ++startingLine) {
        if (tracksAcceptAutoPlacedItem(startingLine, itemSpanLength))
            absoluteShortest = std::min(absoluteShortest, maxRunningPositionForSpan(startingLine, itemSpanLength));
    }

    // Step 2: Find first position within tolerance of shortest, starting from the cursor position.
    unsigned smallestMaxPosLine = 0;
    auto autoFlowNextCursorShift = (m_autoFlowNextCursor > maxStartingLine) ? 0 : m_autoFlowNextCursor;
    for (unsigned offsetFromCursor = 0; offsetFromCursor < maxStartingLine; ++offsetFromCursor) {
        auto startingLine = (autoFlowNextCursorShift + offsetFromCursor) % maxStartingLine;
        if (!tracksAcceptAutoPlacedItem(startingLine, itemSpanLength))
            continue;

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

GridLanesResult::StackingAxisAlignmentContainer GridLanesResult::stackingAxisAlignmentContainerForGridItem(const RenderBox& gridItem) const
{
    return m_stackingAxisAlignmentContainers.getOptional(gridItem).value_or(StackingAxisAlignmentContainer { });
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
