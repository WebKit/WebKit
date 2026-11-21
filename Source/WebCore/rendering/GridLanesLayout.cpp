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
#include <wtf/Borrow.h>

namespace WebCore {

static bool isWithinFitTolerance(LayoutUnit position, LayoutUnit highestPosition, GridLanesLayout::ResolvedFitTolerance fitTolerance)
{
    return WTF::holdsAlternative<CSS::Keyword::Infinite>(fitTolerance) || position <= highestPosition + std::get<LayoutUnit>(fitTolerance);
}

// Track sizes are whole LayoutUnits of 1/64px, so tracks of the same size can still differ by one LayoutUnit. For example,
// three 1fr tracks in a 100px container are 2133, 2133 and 2134 LayoutUnits wide.
static bool haveSameUsedSize(LayoutUnit usedSize, LayoutUnit otherUsedSize)
{
    return (usedSize - otherUsedSize).abs() <= LayoutUnit::fromRawValue(1);
}

// The start of the highest skipped space in the track that starts above the item's normal placement and has room for it.
static std::optional<LayoutUnit> highestSkippedSpaceWithRoom(const Vector<GridLanesLayout::SkippedSpace>& trackSkippedSpaces, LayoutUnit normalPlacementPosition, LayoutUnit spaceNeeded)
{
    for (auto skippedSpace : trackSkippedSpaces) {
        if (skippedSpace.start >= normalPlacementPosition)
            break;
        if (skippedSpace.start + spaceNeeded <= skippedSpace.end)
            return skippedSpace.start;
    }
    return std::nullopt;
}

// "If there are multiple valid spaces within the tie threshold of the highest space, place it in the start-most of them."
static std::optional<unsigned> startMostTrackWithinTieThreshold(const Vector<std::optional<LayoutUnit>>& skippedSpacePositions, GridLanesLayout::ResolvedFitTolerance fitTolerance)
{
    std::optional<LayoutUnit> highestPosition;
    for (auto position : skippedSpacePositions) {
        if (position && (!highestPosition || *position < *highestPosition))
            highestPosition = position;
    }
    if (!highestPosition)
        return std::nullopt;

    for (unsigned track = 0; track < skippedSpacePositions.size(); ++track) {
        auto position = skippedSpacePositions[track];
        if (position && isWithinFitTolerance(*position, *highestPosition, fitTolerance))
            return track;
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

// The item fills the top of the skipped space, and the rest of the space stays skipped.
static void fillSkippedSpace(Vector<GridLanesLayout::SkippedSpace>& trackSkippedSpaces, LayoutUnit position, LayoutUnit spaceNeeded)
{
    for (size_t index = 0; index < trackSkippedSpaces.size(); ++index) {
        auto skippedSpace = trackSkippedSpaces[index];
        if (skippedSpace.start != position)
            continue;
        if (position + spaceNeeded < skippedSpace.end)
            borrow(trackSkippedSpaces)[index].start = position + spaceNeeded;
        else
            trackSkippedSpaces.removeAt(index);
        return;
    }
    ASSERT_NOT_REACHED();
}

GridLanesLayout::GridLanesLayout(RenderGrid& renderGrid, unsigned gridAxisTracksCount, Style::GridTrackSizingDirection stackingAxisDirection)
    : m_runningPositions(gridAxisTracksCount)
    , m_renderGrid(renderGrid)
    , m_stackingAxisGridGap(renderGrid.gridGap(stackingAxisDirection))
    , m_stackingAxisDirection(stackingAxisDirection)
{
    m_renderGrid->currentGrid().setupForGridLanesLayout();
    m_renderGrid->populateExplicitGridAndOrderIterator();
}

GridLanesResult GridLanesLayout::performGridLanesPlacement(const GridTrackSizingAlgorithm& algorithm, ResolvedFitTolerance fitTolerance, Phase layoutPhase, Packing packing)
{
    // 4.4 Grid Lanes Layout and Placement Algorithm
    // https://drafts.csswg.org/css-grid-3/#grid-lanes-layout-algorithm
    return placeGridLanesItems(algorithm, fitTolerance, layoutPhase, packing);
}

GridLanesResult GridLanesLayout::placeGridLanesItems(const GridTrackSizingAlgorithm& algorithm, ResolvedFitTolerance fitTolerance, Phase layoutPhase, Packing packing)
{
    if (!gridAxisTracksCount())
        return { };

    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> stackingAxisOffsets;
    LayoutUnit gridContentSize;
    SkippedSpaces skippedSpaces(packing == Packing::Dense ? gridAxisTracksCount() : 0);

    auto& grid = m_renderGrid->currentGrid();
    for (CheckedRef gridItem : grid.orderIterator().gridItems()) {
        bool isAutoPlacedInGridAxis = !hasDefiniteGridAxisPosition(gridItem, gridAxisDirection());
        auto gridArea = isAutoPlacedInGridAxis ? gridAreaForIndefiniteGridAxisItem(gridItem, fitTolerance) : gridAreaForDefiniteGridAxisItem(gridItem);
        layoutGridItem(algorithm, gridItem, gridArea, layoutPhase);
        auto marginBoxSize = stackingAxisMarginBoxForItem(gridItem);

        auto itemPlacement = packing == Packing::Dense ? placeItemDensely(skippedSpaces, algorithm, gridArea, marginBoxSize, isAutoPlacedInGridAxis, fitTolerance) : placeItemSparsely(gridArea, marginBoxSize, isAutoPlacedInGridAxis);
        // FIXME: A backfilled item goes after the items placed below it in its cell, so the first item of a cell may not be its highest one.
        grid.insert(gridItem, itemPlacement.area);

        stackingAxisOffsets.set(gridItem.get(), itemPlacement.placement.marginBoxStart);
        gridContentSize = std::max(gridContentSize, itemPlacement.placement.marginBoxEnd);
    }

    return { WTF::move(stackingAxisOffsets), gridContentSize };
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

    // The item is inserted into the grid once dense packing has settled its tracks.
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

// Sparse packing places the item at the running position of its tracks.
GridLanesLayout::ItemPlacement GridLanesLayout::placeItemSparsely(const GridArea& area, LayoutUnit marginBoxSize, bool isAutoPlacedInGridAxis)
{
    auto placement = updateRunningPositions(area, marginBoxSize);
    if (isAutoPlacedInGridAxis)
        m_autoFlowNextCursor = gridAxisSpanFromArea(area).endLine() % gridAxisTracksCount();
    return { area, placement };
}

// Dense packing places the item into a skipped space with room for it. Otherwise it places the item sparsely and keeps the spaces the item skips.
GridLanesLayout::ItemPlacement GridLanesLayout::placeItemDensely(SkippedSpaces& skippedSpaces, const GridTrackSizingAlgorithm& algorithm, const GridArea& normalPlacementArea, LayoutUnit marginBoxSize, bool isAutoPlacedInGridAxis, ResolvedFitTolerance fitTolerance)
{
    // "Rewind the auto-placement cursor and the running position to their values before this item's placement."
    if (auto placement = placeIntoSkippedSpace(skippedSpaces, algorithm, normalPlacementArea, marginBoxSize, isAutoPlacedInGridAxis, fitTolerance))
        return *placement;

    recordSkippedSpaces(skippedSpaces, gridAxisSpanFromArea(normalPlacementArea));
    return placeItemSparsely(normalPlacementArea, marginBoxSize, isAutoPlacedInGridAxis);
}

// "If the grid lanes container uses dense packing, and there exists skipped spaces in the layout (e.g. due to spanning items)
// into which the item, as it is sized now, could have fit if it were placed earlier, and where the spanned tracks have the same
// total used size as the tracks into which it is currently placed, then instead place it into the highest such space.
// If there are multiple valid spaces within the tie threshold of the highest space, place it in the start-most of them."
std::optional<GridLanesLayout::ItemPlacement> GridLanesLayout::placeIntoSkippedSpace(SkippedSpaces& skippedSpaces, const GridTrackSizingAlgorithm& algorithm, const GridArea& normalPlacementArea, LayoutUnit marginBoxSize, bool isAutoPlacedInGridAxis, ResolvedFitTolerance fitTolerance)
{
    auto normalPlacementTracks = gridAxisSpanFromArea(normalPlacementArea);
    // FIXME: Place items that span several tracks into skipped spaces.
    if (normalPlacementTracks.integerSpan() != 1)
        return std::nullopt;

    auto spaceNeeded = std::max(0_lu, marginBoxSize + m_stackingAxisGridGap);
    const auto positions = skippedSpacePositions(skippedSpaces, algorithm, normalPlacementTracks, spaceNeeded, isAutoPlacedInGridAxis);
    auto track = startMostTrackWithinTieThreshold(positions, fitTolerance);
    if (!track)
        return std::nullopt;

    auto position = *positions[*track];
    fillSkippedSpace(borrow(skippedSpaces)[*track], position, spaceNeeded);
    return ItemPlacement {
        gridAreaFromGridAxisSpan(GridSpan::translatedDefiniteGridSpan(*track, *track + 1)),
        { position, position + marginBoxSize }
    };
}

// For each track the item could move to, the highest skipped space with room for it. The track needs the same used size as the
// item's normal placement. An item with a definite position can only move within its own track.
Vector<std::optional<LayoutUnit>> GridLanesLayout::skippedSpacePositions(const SkippedSpaces& skippedSpaces, const GridTrackSizingAlgorithm& algorithm, const GridSpan& normalPlacementTracks, LayoutUnit spaceNeeded, bool isAutoPlacedInGridAxis) const
{
    auto& tracks = algorithm.tracks(gridAxisDirection());
    auto normalPlacementTrack = normalPlacementTracks.startLine();
    auto usedSize = tracks[normalPlacementTrack]->baseSize();
    auto normalPlacementPosition = m_runningPositions[normalPlacementTrack];

    Vector<std::optional<LayoutUnit>> positions;
    for (unsigned track = 0; track < gridAxisTracksCount(); ++track) {
        bool canMoveToTrack = isAutoPlacedInGridAxis ? tracksAcceptAutoPlacedItem(track, 1) : track == normalPlacementTrack;
        if (canMoveToTrack && haveSameUsedSize(tracks[track]->baseSize(), usedSize))
            positions.append(highestSkippedSpaceWithRoom(skippedSpaces[track], normalPlacementPosition, spaceNeeded));
        else
            positions.append(std::nullopt);
    }
    return positions;
}

// A spanning item skips the space above itself in each of its tracks whose running position is above the item.
void GridLanesLayout::recordSkippedSpaces(SkippedSpaces& skippedSpaces, const GridSpan& itemTracks) const
{
    auto itemPosition = maxRunningPositionForSpan(itemTracks.startLine(), itemTracks.integerSpan());
    for (auto track : itemTracks) {
        LayoutUnit runningPosition = m_runningPositions[track];
        if (runningPosition < itemPosition)
            borrow(skippedSpaces)[track].append({ runningPosition, itemPosition });
    }
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
        if (isWithinFitTolerance(maxPosForCurrentStartingLine, absoluteShortest, fitTolerance)) {
            smallestMaxPosLine = startingLine;
            break;
        }
    }

    auto gridAxisPosition = GridSpan::translatedDefiniteGridSpan(smallestMaxPosLine, smallestMaxPosLine + itemSpanLength);
    return gridAreaFromGridAxisSpan(gridAxisPosition);
}

LayoutUnit GridLanesResult::stackingAxisOffsetForGridItem(const RenderBox& gridItem) const
{
    return m_stackingAxisOffsets.getOptional(gridItem).value_or(0_lu);
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
