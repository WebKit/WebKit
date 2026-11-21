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

// The skipped space in the track that has room for the item at the position.
static std::optional<size_t> skippedSpaceWithRoom(const Vector<GridLanesLayout::SkippedSpace>& trackSkippedSpaces, LayoutUnit position, LayoutUnit spaceNeeded)
{
    for (size_t index = 0; index < trackSkippedSpaces.size(); ++index) {
        auto skippedSpace = trackSkippedSpaces[index];
        if (skippedSpace.start <= position && position + spaceNeeded <= skippedSpace.end)
            return index;
    }
    return std::nullopt;
}

static bool hasRoomInEveryTrack(std::span<const Vector<GridLanesLayout::SkippedSpace>> itemTracksSkippedSpaces, std::span<const LayoutUnit> itemTracksRunningPositions, LayoutUnit position, LayoutUnit spaceNeeded)
{
    for (size_t track = 0; track < itemTracksSkippedSpaces.size(); ++track) {
        if (position < itemTracksRunningPositions[track] && !skippedSpaceWithRoom(itemTracksSkippedSpaces[track], position, spaceNeeded))
            return false;
    }
    return true;
}

// The highest position above the limit where each of the item's tracks has room for it, in a skipped space or below the track's
// running position. The highest such position is always where one of these spaces starts.
static std::optional<LayoutUnit> highestSkippedSpaceWithRoom(std::span<const Vector<GridLanesLayout::SkippedSpace>> itemTracksSkippedSpaces, std::span<const LayoutUnit> itemTracksRunningPositions, LayoutUnit positionLimit, LayoutUnit spaceNeeded)
{
    Vector<LayoutUnit> positions;
    for (size_t track = 0; track < itemTracksSkippedSpaces.size(); ++track) {
        positions.append(itemTracksRunningPositions[track]);
        for (auto skippedSpace : itemTracksSkippedSpaces[track])
            positions.append(skippedSpace.start);
    }

    std::optional<LayoutUnit> highestPosition;
    for (auto position : positions) {
        if (position < positionLimit && (!highestPosition || position < *highestPosition) && hasRoomInEveryTrack(itemTracksSkippedSpaces, itemTracksRunningPositions, position, spaceNeeded))
            highestPosition = position;
    }
    return highestPosition;
}

// "If there are multiple valid spaces within the tie threshold of the highest space, place it in the start-most of them."
static std::optional<unsigned> startMostLineWithinTieThreshold(const Vector<std::optional<LayoutUnit>>& skippedSpacePositions, GridLanesLayout::ResolvedFitTolerance fitTolerance)
{
    std::optional<LayoutUnit> highestPosition;
    for (auto position : skippedSpacePositions) {
        if (position && (!highestPosition || *position < *highestPosition))
            highestPosition = position;
    }
    if (!highestPosition)
        return std::nullopt;

    for (unsigned startLine = 0; startLine < skippedSpacePositions.size(); ++startLine) {
        auto position = skippedSpacePositions[startLine];
        if (position && isWithinFitTolerance(*position, *highestPosition, fitTolerance))
            return startLine;
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

// The item fills part of the skipped space, and the parts above and below it stay skipped.
static void fillSkippedSpace(Vector<GridLanesLayout::SkippedSpace>& trackSkippedSpaces, LayoutUnit position, LayoutUnit spaceNeeded)
{
    auto index = skippedSpaceWithRoom(trackSkippedSpaces, position, spaceNeeded);
    if (!index) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto skippedSpace = trackSkippedSpaces[*index];
    trackSkippedSpaces.removeAt(*index);
    if (position + spaceNeeded < skippedSpace.end)
        trackSkippedSpaces.insert(*index, { position + spaceNeeded, skippedSpace.end });
    if (skippedSpace.start < position)
        trackSkippedSpaces.insert(*index, { skippedSpace.start, position });
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

    struct PlacedItem {
        CheckedRef<const RenderBox> gridItem;
        GridSpan gridAxisTracks;
        LayoutUnit marginBoxStart;
    };
    Vector<PlacedItem> placedItems;
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

        placedItems.append({ gridItem, clampedGridAxisTracks(itemPlacement.area), itemPlacement.placement.marginBoxStart });
        gridContentSize = std::max(gridContentSize, itemPlacement.placement.marginBoxEnd);
    }

    // A backfilled item is placed after the items below it in its tracks.
    if (packing == Packing::Dense)
        std::ranges::stable_sort(placedItems, { }, &PlacedItem::marginBoxStart);

    // "For the last item in a track, the alignment container extends to the lowest bottom outer edge among all the last items in all tracks."
    auto lowestLastItemEdge = std::ranges::max(m_runningPositions) - m_stackingAxisGridGap;
    Vector<LayoutUnit> alignmentContainerEnds(FillWith { }, gridAxisTracksCount(), lowestLastItemEdge);
    HashMap<SingleThreadWeakRef<const RenderBox>, GridLanesResult::StackingAxisAlignmentContainer> stackingAxisAlignmentContainers;
    // "In the stacking axis, the self-alignment properties only apply to items that are adjacent to a “gap” in the layout,
    // i.e. placed in their grid track(s) either immediately before a spanning item or as the last item."
    for (auto placedItem : std::views::reverse(placedItems)) {
        auto alignmentContainerEnd = lowestLastItemEdge;
        // Walking backwards, alignmentContainerEnds[track] is where the next item in the track starts, less the gap.
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

GridSpan GridLanesLayout::clampedGridAxisTracks(const GridArea& area) const
{
    auto gridAxisTracks = gridAxisSpanFromArea(area);
    ASSERT(gridAxisTracks.startLine() < m_runningPositions.size() && gridAxisTracks.endLine() <= m_runningPositions.size());
    gridAxisTracks.clamp(m_runningPositions.size());
    return gridAxisTracks;
}

GridLanesLayout::StackingAxisPlacement GridLanesLayout::updateRunningPositions(const GridArea& area, LayoutUnit marginBoxSize)
{
    auto gridAxisSpan = clampedGridAxisTracks(area);

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

    recordSkippedSpaces(skippedSpaces, clampedGridAxisTracks(normalPlacementArea));
    return placeItemSparsely(normalPlacementArea, marginBoxSize, isAutoPlacedInGridAxis);
}

// "If the grid lanes container uses dense packing, and there exists skipped spaces in the layout (e.g. due to spanning items)
// into which the item, as it is sized now, could have fit if it were placed earlier, and where the spanned tracks have the same
// total used size as the tracks into which it is currently placed, then instead place it into the highest such space.
// If there are multiple valid spaces within the tie threshold of the highest space, place it in the start-most of them."
std::optional<GridLanesLayout::ItemPlacement> GridLanesLayout::placeIntoSkippedSpace(SkippedSpaces& skippedSpaces, const GridTrackSizingAlgorithm& algorithm, const GridArea& normalPlacementArea, LayoutUnit marginBoxSize, bool isAutoPlacedInGridAxis, ResolvedFitTolerance fitTolerance)
{
    auto normalPlacementTracks = clampedGridAxisTracks(normalPlacementArea);
    auto trackCount = normalPlacementTracks.integerSpan();
    auto spaceNeeded = std::max(0_lu, marginBoxSize + m_stackingAxisGridGap);
    const auto positions = skippedSpacePositions(skippedSpaces, algorithm, normalPlacementTracks, spaceNeeded, isAutoPlacedInGridAxis);
    auto startLine = startMostLineWithinTieThreshold(positions, fitTolerance);
    if (!startLine)
        return std::nullopt;

    auto position = *positions[*startLine];
    for (auto track = *startLine; track < *startLine + trackCount; ++track) {
        if (position >= m_runningPositions[track])
            fillBelowRunningPosition(skippedSpaces, track, position, marginBoxSize);
        else
            fillSkippedSpace(borrow(skippedSpaces)[track], position, spaceNeeded);
    }
    return ItemPlacement {
        gridAreaFromGridAxisSpan(GridSpan::translatedDefiniteGridSpan(*startLine, *startLine + trackCount)),
        { position, position + marginBoxSize }
    };
}

// For each start line the item could move to, the highest position where its tracks have room for it, with at least one of them in
// a skipped space. The tracks need the same total used size as the item's normal placement. An item with a definite position can
// only move within its own tracks.
Vector<std::optional<LayoutUnit>> GridLanesLayout::skippedSpacePositions(const SkippedSpaces& skippedSpaces, const GridTrackSizingAlgorithm& algorithm, const GridSpan& normalPlacementTracks, LayoutUnit spaceNeeded, bool isAutoPlacedInGridAxis) const
{
    auto trackCount = normalPlacementTracks.integerSpan();
    auto normalPlacementLine = normalPlacementTracks.startLine();
    // FIXME: The intrinsic width pass sizes these tracks without the container's definite height, so it can backfill differently than layout.
    auto usedSize = usedSizeOfTracks(algorithm, normalPlacementLine, trackCount);
    auto normalPlacementPosition = maxRunningPositionForSpan(normalPlacementLine, trackCount);

    Vector<std::optional<LayoutUnit>> positions;
    for (unsigned startLine = 0; startLine + trackCount <= gridAxisTracksCount(); ++startLine) {
        bool canMoveToTracks = isAutoPlacedInGridAxis ? tracksAcceptAutoPlacedItem(startLine, trackCount) : startLine == normalPlacementLine;
        std::optional<LayoutUnit> position;
        if (canMoveToTracks && usedSizeOfTracks(algorithm, startLine, trackCount) == usedSize) {
            // Below the highest running position of the tracks, at least one of them has to use a skipped space.
            auto positionLimit = std::min(normalPlacementPosition, maxRunningPositionForSpan(startLine, trackCount));
            position = highestSkippedSpaceWithRoom(skippedSpaces.subspan(startLine, trackCount), m_runningPositions.subspan(startLine, trackCount), positionLimit, spaceNeeded);
        }
        positions.append(position);
    }
    return positions;
}

// The item fills the open end of the track, below its running position. The space above the item becomes a skipped space.
void GridLanesLayout::fillBelowRunningPosition(SkippedSpaces& skippedSpaces, unsigned track, LayoutUnit position, LayoutUnit marginBoxSize)
{
    LayoutUnit runningPosition = m_runningPositions[track];
    if (runningPosition < position)
        borrow(skippedSpaces)[track].append({ runningPosition, position });
    // FIXME: The spec rewinds the running position after dense placement, which would let later items in this track overlap the item.
    m_runningPositions[track] = position + std::max(0_lu, marginBoxSize) + m_stackingAxisGridGap;
}

LayoutUnit GridLanesLayout::usedSizeOfTracks(const GridTrackSizingAlgorithm& algorithm, unsigned startLine, unsigned trackCount) const
{
    auto& tracks = algorithm.tracks(gridAxisDirection());
    LayoutUnit usedSize;
    for (auto track = startLine; track < startLine + trackCount; ++track)
        usedSize += tracks[track]->baseSize();
    return usedSize;
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
