/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ImplicitGrid.h"

#include "GridFormattingContext.h"
#include "PlacedGridItem.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "UnplacedGridItem.h"
#include <wtf/Assertions.h>

namespace WebCore {
namespace Layout {

// The implicit grid is created from the explicit grid + items that are placed outside
// of the explicit grid. Since we know the explicit tracks from style we start the
// implicit grid as exactly the explicit grid and allow placement to add implicit
// tracks and grow the grid.

ImplicitGrid::ImplicitGrid(size_t totalNonAutoFlowTracksCount, size_t totalAutoFlowTracksCount, size_t maxNonAutoFlowTracksCount, size_t maxAutoFlowTracksCount)
    : m_gridMatrix(GridMatrix(FillWith { }, totalAutoFlowTracksCount, AutoFlowTrack(totalNonAutoFlowTracksCount)))
    , m_nonAutoFlowTracksCount(totalNonAutoFlowTracksCount)
    , m_maxNonAutoFlowTracksCount(maxNonAutoFlowTracksCount)
    , m_maxAutoFlowTracksCount(maxAutoFlowTracksCount)
{
    ASSERT(maxNonAutoFlowTracksCount && maxAutoFlowTracksCount);
}

// https://drafts.csswg.org/css-grid-1/#overlarge-grids
// "If the grid area would span outside the limited grid, its span is clamped to the last line of the limited grid."
// "If the grid area would be placed completely outside the limited grid, its span must be truncated to 1 and the area
// repositioned into the last grid track on that side of the grid."
static WTF::Range<size_t> clampedToLimit(size_t start, size_t span, size_t limit)
{
    return { std::min(start, limit - 1), std::min(start + span, limit) };
}

// https://drafts.csswg.org/css-grid-1/#overlarge-grids
// "If the grid area would span outside the limited grid, its span is clamped to the last line of the limited grid."
// Style clamps a span to the limit in effect when the style was resolved, which can be a higher one than the limit used for this layout.
size_t ImplicitGrid::nonAutoFlowSpanLimitedToGrid(const UnplacedGridItem& item) const
{
    return std::min(item.nonAutoFlowSpanSize(), m_maxNonAutoFlowTracksCount);
}

struct GridDimensions {
    size_t totalNonAutoFlowTracksCount { 0 };
    size_t totalAutoFlowTracksCount { 0 };
};

static GridDimensions calculateInitialImplicitGridDimensions(const UnplacedGridItems& unplacedGridItems, size_t leadingNonAutoFlowTracksCount, size_t leadingAutoFlowTracksCount, size_t explicitNonAutoFlowTracksCount, size_t explicitAutoFlowTracksCount)
{
    // The explicit grid is preceded by any leading implicit tracks generated for items placed with
    // a negative line that resolves before the grid start. Every item's line has already been
    // shifted forward by this amount, so include the leading tracks in the initial bounds.
    size_t maximumNonAutoFlowIndex = explicitNonAutoFlowTracksCount + leadingNonAutoFlowTracksCount;
    size_t maximumAutoFlowIndex = explicitAutoFlowTracksCount + leadingAutoFlowTracksCount;

    auto updateGridBounds = [&](const UnplacedGridItem& item) {
        if (item.hasDefiniteAutoFlowPosition()) {
            auto autoFlowRange = item.definiteAutoFlowRange();
            maximumAutoFlowIndex = std::max({ maximumAutoFlowIndex, autoFlowRange.begin(), autoFlowRange.end() });
        }

        if (item.hasDefiniteNonAutoFlowPosition()) {
            auto nonAutoFlowRange = item.definiteNonAutoFlowRange();
            maximumNonAutoFlowIndex = std::max({ maximumNonAutoFlowIndex, nonAutoFlowRange.begin(), nonAutoFlowRange.end() });
        }
    };

    for (const auto& item : unplacedGridItems.nonAutoPositionedItems)
        updateGridBounds(item);
    for (const auto& item : unplacedGridItems.definiteAutoFlowPositionedItems)
        updateGridBounds(item);

    return {
        maximumNonAutoFlowIndex,
        maximumAutoFlowIndex
    };
}

ImplicitGrid ImplicitGrid::createInitialGrid(const UnplacedGridItems& unplacedGridItems, size_t leadingNonAutoFlowTracksCount, size_t leadingAutoFlowTracksCount, size_t explicitNonAutoFlowTracksCount, size_t explicitAutoFlowTracksCount, size_t maxTracks)
{
    auto initialDimensions = calculateInitialImplicitGridDimensions(
        unplacedGridItems, leadingNonAutoFlowTracksCount, leadingAutoFlowTracksCount, explicitNonAutoFlowTracksCount, explicitAutoFlowTracksCount);

    // The limited grid has the lines [1 - maxTracks, maxTracks] around the explicit grid, which is where the leading implicit tracks end.
    // There are at most maxTracks - 1 leading implicit tracks, so the grid has fewer than 2 * maxTracks tracks per axis.
    ImplicitGrid implicitGrid(initialDimensions.totalNonAutoFlowTracksCount, initialDimensions.totalAutoFlowTracksCount, leadingNonAutoFlowTracksCount + maxTracks, leadingAutoFlowTracksCount + maxTracks);
    // 3. Determine the non-auto-flow tracks in the implicit grid (the columns for grid-auto-flow: row).
    implicitGrid.determineImplicitGridNonAutoFlowTracks(unplacedGridItems.autoPositionedItems);

    return implicitGrid;
}

FlowRelativeAreaLines ImplicitGrid::insertUnplacedGridItem(const UnplacedGridItem& unplacedGridItem)
{
    // https://drafts.csswg.org/css-grid/#common-uses-numeric
    // The initial grid bounds already cover every definitely placed item, spans included.
    return markAreaAsOccupied(unplacedGridItem.definiteNonAutoFlowRange(), unplacedGridItem.definiteAutoFlowRange());
}

FlowRelativeAreaLines ImplicitGrid::insertDefiniteAutoFlowItem(const UnplacedGridItem& unplacedGridItem, GridAutoFlowOptions autoFlowOptions)
{
    // Step 2 of CSS Grid auto-placement algorithm:
    // Process items locked to a given auto-flow track (definite auto-flow position, auto non-auto-flow position)
    // See: https://www.w3.org/TR/css-grid-1/#auto-placement-algo

    ASSERT(unplacedGridItem.hasDefiniteAutoFlowPosition() && !unplacedGridItem.hasDefiniteNonAutoFlowPosition());
    auto autoFlowRange = unplacedGridItem.definiteAutoFlowRange();

    auto nonAutoFlowSpan = nonAutoFlowSpanLimitedToGrid(unplacedGridItem);
    std::optional<size_t> nonAutoFlowPosition = findNonAutoFlowPositionForDefiniteAutoFlowItem(autoFlowRange, nonAutoFlowSpan, autoFlowOptions);

    WTF::Range<size_t> nonAutoFlowRange;
    if (!nonAutoFlowPosition) {
        auto nonAutoFlowStart = growNonAutoFlowTracksForDefiniteAutoFlowItem(nonAutoFlowSpan, autoFlowRange);

        // Retry finding position in the grown grid
        nonAutoFlowPosition = findNonAutoFlowPositionForDefiniteAutoFlowItem(autoFlowRange, nonAutoFlowSpan, autoFlowOptions);
        if (nonAutoFlowPosition) {
            nonAutoFlowRange = { *nonAutoFlowPosition, *nonAutoFlowPosition + nonAutoFlowSpan };
            ASSERT(isCellRangeEmpty(nonAutoFlowRange, autoFlowRange));
        } else {
            // The grid could not grow to hold the item because it is at its limit. The item goes where it would have gone
            // in a grid without one, clamped to the limited grid, where it can overlap other items.
            ASSERT(nonAutoFlowTracksCount() == m_maxNonAutoFlowTracksCount);
            nonAutoFlowRange = clampedToLimit(nonAutoFlowStart, nonAutoFlowSpan, m_maxNonAutoFlowTracksCount);
        }
    } else {
        nonAutoFlowRange = { *nonAutoFlowPosition, *nonAutoFlowPosition + nonAutoFlowSpan };
        ASSERT(isCellRangeEmpty(nonAutoFlowRange, autoFlowRange));
    }
    auto gridAreaLines = markAreaAsOccupied(nonAutoFlowRange, autoFlowRange);

    if (autoFlowOptions.strategy != PackingStrategy::Dense) {
        for (auto autoFlowTrack : std::views::iota(autoFlowRange.begin(), autoFlowRange.end()))
            m_autoFlowTrackCursors.set(autoFlowTrack, nonAutoFlowRange.end());
    }

    return gridAreaLines;
}

// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
// Step 3: Determine the non-auto-flow tracks in the implicit grid (the columns for grid-auto-flow: row).
void ImplicitGrid::determineImplicitGridNonAutoFlowTracks(const Vector<UnplacedGridItem>& autoPositionedItems)
{
    size_t requiredNonAutoFlowTracks = nonAutoFlowTracksCount();

    // Part 1: "Among all the items with a definite column position, add columns to the end
    // of the implicit grid as necessary to accommodate those items." (the non-auto-flow position)
    for (auto& item : autoPositionedItems) {
        if (item.hasDefiniteNonAutoFlowPosition())
            requiredNonAutoFlowTracks = std::max(requiredNonAutoFlowTracks, item.definiteNonAutoFlowRange().end());
    }

    // Part 2: "If the largest column span among all the items without a definite column position
    // is larger than the width of the implicit grid, add columns to accommodate that column span."
    // (the non-auto-flow span and position)
    size_t maxNonAutoFlowSpan = 0;
    for (auto& item : autoPositionedItems) {
        if (!item.hasDefiniteNonAutoFlowPosition())
            maxNonAutoFlowSpan = std::max(maxNonAutoFlowSpan, nonAutoFlowSpanLimitedToGrid(item));
    }
    requiredNonAutoFlowTracks = std::max(requiredNonAutoFlowTracks, maxNonAutoFlowSpan);

    // Grow grid once to accommodate both requirements
    if (requiredNonAutoFlowTracks > nonAutoFlowTracksCount())
        growNonAutoFlowTracksToFit(requiredNonAutoFlowTracks);
}

// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
// Step 4 of CSS Grid auto-placement algorithm: Position the remaining grid items
FlowRelativeAreaLines ImplicitGrid::insertAutoPositionedItem(const UnplacedGridItem& item, GridAutoFlowOptions autoFlowOptions)
{
    if (autoFlowOptions.direction != GridAutoFlowDirection::Row) {
        ASSERT_NOT_IMPLEMENTED_YET();
        // Not a real placement: column flow is rejected by grid coverage before placement runs.
        // Return a single cell rather than an empty area, since a zero span underflows the interior
        // gutter count in GridLinePositions::areaSize().
        return { 0, 1, 0, 1 };
    }

    if (item.hasDefiniteNonAutoFlowPosition())
        return placeAutoPositionedItemWithDefiniteNonAutoFlowPosition(item, autoFlowOptions);
    return placeAutoPositionedItemWithAutoPositionInBothAxes(item, autoFlowOptions);
}

std::optional<size_t> ImplicitGrid::findFirstAvailableNonAutoFlowPosition(WTF::Range<size_t> autoFlowRange, size_t nonAutoFlowSpan, size_t startSearchPosition) const
{
    auto currentNonAutoFlowTracksCount = nonAutoFlowTracksCount();

    // If we can't fit the span starting from the search position, signal that we need to grow the grid
    if (startSearchPosition + nonAutoFlowSpan > currentNonAutoFlowTracksCount)
        return std::nullopt;

    // Search within existing grid bounds
    for (size_t nonAutoFlowStart = startSearchPosition; nonAutoFlowStart <= currentNonAutoFlowTracksCount - nonAutoFlowSpan; ++nonAutoFlowStart) {
        if (isCellRangeEmpty({ nonAutoFlowStart, nonAutoFlowStart + nonAutoFlowSpan }, autoFlowRange))
            return nonAutoFlowStart;
    }
    // If we are unable to find a valid position, signal that we need to grow the grid.
    return std::nullopt;
}

std::optional<size_t> ImplicitGrid::findNonAutoFlowPositionForDefiniteAutoFlowItem(WTF::Range<size_t> autoFlowRange, size_t nonAutoFlowSpan, GridAutoFlowOptions autoFlowOptions) const
{
    if (autoFlowOptions.strategy == PackingStrategy::Dense) {
        // Dense packing: always start searching from the first non-auto-flow track
        return findFirstAvailableNonAutoFlowPosition(autoFlowRange, nonAutoFlowSpan, 0);
    }
    // Sparse packing: use per-auto-flow-track cursors to maintain placement order
    // For items spanning several auto-flow tracks, use the maximum cursor position across all spanned tracks
    ASSERT(autoFlowOptions.strategy == PackingStrategy::Sparse);
    size_t startSearchPosition = 0;
    for (auto autoFlowTrack : std::views::iota(autoFlowRange.begin(), autoFlowRange.end()))
        startSearchPosition = std::max(startSearchPosition, m_autoFlowTrackCursors.get(autoFlowTrack));
    return findFirstAvailableNonAutoFlowPosition(autoFlowRange, nonAutoFlowSpan, startSearchPosition);
}

size_t ImplicitGrid::growNonAutoFlowTracksForDefiniteAutoFlowItem(size_t nonAutoFlowSpan, WTF::Range<size_t> autoFlowRange)
{
    // Only reached when the item does not fit in any of the existing non-auto-flow tracks, so it has to go
    // past everything already in the auto-flow tracks it spans. An earlier gap is no use: the item needs the
    // same non-auto-flow tracks free in every spanned auto-flow track, and the caller already searched for such a run.
    // The track is optional so that auto-flow tracks which are still empty stay distinguishable from ones
    // whose very first cell is occupied.
    std::optional<size_t> lastOccupiedNonAutoFlowTrack;
    for (auto autoFlowTrack : std::views::iota(autoFlowRange.begin(), autoFlowRange.end())) {
        for (size_t nonAutoFlowTrack = nonAutoFlowTracksCount(); nonAutoFlowTrack > 0; --nonAutoFlowTrack) {
            if (m_gridMatrix[autoFlowTrack].quickGet(nonAutoFlowTrack - 1)) {
                lastOccupiedNonAutoFlowTrack = std::max(lastOccupiedNonAutoFlowTrack.value_or(0), nonAutoFlowTrack - 1);
                break;
            }
        }
    }

    // The item starts right after the last occupied cell, or at the very first non-auto-flow track when none
    // of the spanned auto-flow tracks hold anything yet.
    auto nonAutoFlowStart = lastOccupiedNonAutoFlowTrack ? *lastOccupiedNonAutoFlowTrack + 1 : 0;
    growNonAutoFlowTracksToFit(nonAutoFlowStart + nonAutoFlowSpan);
    return nonAutoFlowStart;
}

bool ImplicitGrid::isCellRangeEmpty(WTF::Range<size_t> nonAutoFlowRange, WTF::Range<size_t> autoFlowRange) const
{
    // Cells past the limited grid do not exist, so they are never occupied.
    auto autoFlowEnd = std::min(autoFlowRange.end(), autoFlowTracksCount());
    auto nonAutoFlowEnd = std::min(nonAutoFlowRange.end(), nonAutoFlowTracksCount());
    if (autoFlowRange.begin() >= autoFlowEnd || nonAutoFlowRange.begin() >= nonAutoFlowEnd)
        return true;
    for (auto autoFlowTrack : std::views::iota(autoFlowRange.begin(), autoFlowEnd)) {
        for (auto nonAutoFlowTrack : std::views::iota(nonAutoFlowRange.begin(), nonAutoFlowEnd)) {
            if (m_gridMatrix[autoFlowTrack].quickGet(nonAutoFlowTrack))
                return false;
        }
    }
    return true;
}

FlowRelativeAreaLines ImplicitGrid::markAreaAsOccupied(WTF::Range<size_t> nonAutoFlowRange, WTF::Range<size_t> autoFlowRange)
{
    // Every caller is responsible for growing the implicit grid to cover the area first.
    ASSERT(nonAutoFlowRange.end() <= nonAutoFlowTracksCount() && autoFlowRange.end() <= autoFlowTracksCount());

    for (auto autoFlowTrack : std::views::iota(autoFlowRange.begin(), autoFlowRange.end())) {
        for (auto nonAutoFlowTrack : std::views::iota(nonAutoFlowRange.begin(), nonAutoFlowRange.end()))
            m_gridMatrix[autoFlowTrack].quickSet(nonAutoFlowTrack);
    }

    return { nonAutoFlowRange.begin(), nonAutoFlowRange.end(), autoFlowRange.begin(), autoFlowRange.end() };
}

void ImplicitGrid::growNonAutoFlowTracksToFit(size_t requiredCount)
{
    requiredCount = std::min(requiredCount, m_maxNonAutoFlowTracksCount);
    if (requiredCount > m_nonAutoFlowTracksCount) {
        // ensureSize() zeroes the bits it adds, and bits past the current width are never set, so
        // the widened part of each auto-flow track always reads as unoccupied.
        for (auto& autoFlowTrack : m_gridMatrix)
            autoFlowTrack.ensureSize(requiredCount);
        m_nonAutoFlowTracksCount = requiredCount;
    }
}

void ImplicitGrid::growAutoFlowTracksToFit(size_t requiredAutoFlowIndex)
{
    requiredAutoFlowIndex = std::min(requiredAutoFlowIndex, m_maxAutoFlowTracksCount - 1);
    while (requiredAutoFlowIndex >= autoFlowTracksCount())
        m_gridMatrix.append(AutoFlowTrack(m_nonAutoFlowTracksCount));
}

// FIXME: optimize cursor setting by setting to an empty slot instead of to the start for dense placement.
FlowRelativeAreaLines ImplicitGrid::placeAutoPositionedItemWithDefiniteNonAutoFlowPosition(const UnplacedGridItem& item, GridAutoFlowOptions autoFlowOptions)
{
    ASSERT(item.hasDefiniteNonAutoFlowPosition());
    ASSERT(!item.hasDefiniteAutoFlowPosition());

    // Items with definite non-auto-flow position and auto auto-flow position
    // Search along the auto-flow tracks, at the specified non-auto-flow tracks.
    auto nonAutoFlowRange = item.definiteNonAutoFlowRange();
    auto autoFlowSpan = item.autoFlowSpanSize();

    // Step 3 grew the implicit grid to cover the non-auto-flow end line of every item in this step that
    // has a definite non-auto-flow position, so the cells this searches are always in bounds.
    ASSERT(nonAutoFlowRange.end() <= nonAutoFlowTracksCount());

    if (autoFlowOptions.strategy == PackingStrategy::Dense) {
        // Set the auto-flow position of the cursor to the start-most auto-flow line in the implicit grid.
        m_autoPlacementCursorAutoFlowPosition = 0;
    } else {
        // Sparse packing: Check if we would be going backwards (to an earlier non-auto-flow track)
        // If so, advance the auto-flow position to avoid backtracking.
        if (nonAutoFlowRange.begin() < m_autoPlacementCursorNonAutoFlowPosition)
            ++m_autoPlacementCursorAutoFlowPosition;
    }

    // "Set the column position of the cursor to the grid item's column-start line." (the non-auto-flow position and start line)
    m_autoPlacementCursorNonAutoFlowPosition = nonAutoFlowRange.begin();

    // Increment the cursor's auto-flow position until a value is found where the grid item
    // does not overlap any occupied grid cells (creating new auto-flow tracks in the implicit grid as necessary).
    while (true) {
        growAutoFlowTracksToFit(m_autoPlacementCursorAutoFlowPosition + autoFlowSpan - 1);

        WTF::Range<size_t> autoFlowRange { m_autoPlacementCursorAutoFlowPosition, m_autoPlacementCursorAutoFlowPosition + autoFlowSpan };
        if (isCellRangeEmpty(nonAutoFlowRange, autoFlowRange)) {
            // Set the item's auto-flow start line to the cursor's auto-flow position. The cursor stays where the
            // item landed (auto-flow position at the placed track, non-auto-flow position was already set). An item that does not fit gets its area clamped.
            return markAreaAsOccupied(nonAutoFlowRange, clampedToLimit(autoFlowRange.begin(), autoFlowSpan, m_maxAutoFlowTracksCount));
        }

        // Try the next auto-flow track.
        ++m_autoPlacementCursorAutoFlowPosition;
    }
}

// FIXME: optimize cursor setting by setting to an empty slot instead of to the start for dense placement.
FlowRelativeAreaLines ImplicitGrid::placeAutoPositionedItemWithAutoPositionInBothAxes(const UnplacedGridItem& item, GridAutoFlowOptions autoFlowOptions)
{
    ASSERT(!item.hasDefiniteNonAutoFlowPosition() && !item.hasDefiniteAutoFlowPosition());

    auto autoFlowSpan = item.autoFlowSpanSize();
    auto nonAutoFlowSpan = nonAutoFlowSpanLimitedToGrid(item);

    // Step 3 widened the implicit grid to the largest non-auto-flow span among the items in this step
    // without a definite non-auto-flow position (at most the limit), so an auto-flow track always has room for this item
    // and the search below terminates.
    ASSERT(nonAutoFlowSpan <= nonAutoFlowTracksCount());

    // Position items with automatic grid position in both axes.
    // Search along the non-auto-flow tracks, then along the auto-flow tracks.
    if (autoFlowOptions.strategy == PackingStrategy::Dense) {
        // Set the cursor to the start-most position in the implicit grid.
        m_autoPlacementCursorAutoFlowPosition = 0;
        m_autoPlacementCursorNonAutoFlowPosition = 0;
    }

    // Increment the non-auto-flow position of the auto-placement cursor until either this item's grid area
    // does not overlap any occupied grid cells, or the cursor's non-auto-flow position, plus the item's non-auto-flow span,
    // overflow the number of non-auto-flow tracks in the implicit grid, then move the cursor to the start of the next auto-flow track.
    while (true) {
        // Check if we need to move to a new auto-flow track.
        if (m_autoPlacementCursorNonAutoFlowPosition + nonAutoFlowSpan > nonAutoFlowTracksCount()) {
            // Advance to the next auto-flow track, reset the non-auto-flow position to 0.
            ++m_autoPlacementCursorAutoFlowPosition;
            m_autoPlacementCursorNonAutoFlowPosition = 0;
        }

        // Ensure the grid has enough auto-flow tracks before checking if the range is empty.
        growAutoFlowTracksToFit(m_autoPlacementCursorAutoFlowPosition + autoFlowSpan - 1);

        // Try to place at current cursor position.
        WTF::Range<size_t> nonAutoFlowRange { m_autoPlacementCursorNonAutoFlowPosition, m_autoPlacementCursorNonAutoFlowPosition + nonAutoFlowSpan };
        WTF::Range<size_t> autoFlowRange { m_autoPlacementCursorAutoFlowPosition, m_autoPlacementCursorAutoFlowPosition + autoFlowSpan };
        if (isCellRangeEmpty(nonAutoFlowRange, autoFlowRange)) {
            auto gridAreaLines = markAreaAsOccupied(nonAutoFlowRange, clampedToLimit(autoFlowRange.begin(), autoFlowSpan, m_maxAutoFlowTracksCount));
            // Sparse packing: Advance cursor past this item to maintain document order.
            // Spec: "Set the auto-placement cursor to the end of the item's grid area."
            // Dense packing: Cursor will be reset to (0, 0) before the next fully-auto item.
            if (autoFlowOptions.strategy == PackingStrategy::Sparse)
                m_autoPlacementCursorNonAutoFlowPosition += nonAutoFlowSpan;
            return gridAreaLines;
        }
        // Spec: "Increment the column position of the auto-placement cursor." (the non-auto-flow position)
        ++m_autoPlacementCursorNonAutoFlowPosition;
    }
}

} // namespace Layout
} // namespace WebCore
