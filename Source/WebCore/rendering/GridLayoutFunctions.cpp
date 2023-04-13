/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "GridLayoutFunctions.h"

#include "LengthFunctions.h"
#include "RenderGrid.h"

namespace WebCore {

namespace GridLayoutFunctions {

static inline bool marginStartIsAuto(const RenderBox& child, GridTrackSizingDirection direction)
{
    return direction == ForColumns ? child.style().marginStart().isAuto() : child.style().marginBefore().isAuto();
}

static inline bool marginEndIsAuto(const RenderBox& child, GridTrackSizingDirection direction)
{
    return direction == ForColumns ? child.style().marginEnd().isAuto() : child.style().marginAfter().isAuto();
}

static bool childHasMargin(const RenderBox& child, GridTrackSizingDirection direction)
{
    // Length::IsZero returns true for 'auto' margins, which is aligned with the purpose of this function.
    if (direction == ForColumns)
        return !child.style().marginStart().isZero() || !child.style().marginEnd().isZero();
    return !child.style().marginBefore().isZero() || !child.style().marginAfter().isZero();
}

LayoutUnit computeMarginLogicalSizeForChild(const RenderGrid& grid, GridTrackSizingDirection direction, const RenderBox& child)
{
    GridTrackSizingDirection flowAwareDirection = flowAwareDirectionForChild(grid, child, direction);
    if (!childHasMargin(child, flowAwareDirection))
        return 0;

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (direction == ForColumns)
        child.computeInlineDirectionMargins(grid, child.containingBlockLogicalWidthForContentInFragment(nullptr), { }, child.logicalWidth(), marginStart, marginEnd);
    else
        child.computeBlockDirectionMargins(grid, marginStart, marginEnd);
    return marginStartIsAuto(child, flowAwareDirection) ? marginEnd : marginEndIsAuto(child, flowAwareDirection) ? marginStart : marginStart + marginEnd;
}

static bool hasRelativeOrIntrinsicSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    if (direction == ForColumns)
        return child.hasRelativeLogicalWidth() || child.style().logicalWidth().isIntrinsicOrAuto();
    return child.hasRelativeLogicalHeight() || child.style().logicalHeight().isIntrinsicOrAuto();
}

static LayoutUnit extraMarginForSubgrid(const RenderGrid& parent, unsigned startLine, unsigned endLine, GridTrackSizingDirection direction)
{
    unsigned numTracks = parent.numTracks(direction);
    if (!numTracks || !parent.isSubgrid(direction))
        return 0_lu;

    std::optional<LayoutUnit> availableSpace;
    if (!hasRelativeOrIntrinsicSizeForChild(parent, direction))
        availableSpace = parent.availableSpaceForGutters(direction);

    RenderGrid& grandParent = downcast<RenderGrid>(*parent.parent());
    LayoutUnit mbp;
    if (!startLine)
        mbp += (direction == ForColumns) ? parent.marginAndBorderAndPaddingStart() : parent.marginAndBorderAndPaddingBefore();
    else
        mbp += (parent.gridGap(direction, availableSpace) - grandParent.gridGap(direction)) / 2;

    if (endLine == numTracks)
        mbp += (direction == ForColumns) ? parent.marginAndBorderAndPaddingEnd() : parent.marginAndBorderAndPaddingAfter();
    else
        mbp += (parent.gridGap(direction, availableSpace) - grandParent.gridGap(direction)) / 2;

    return mbp;
}

LayoutUnit extraMarginForSubgridAncestors(GridTrackSizingDirection direction, const RenderBox& child)
{
    const RenderGrid* grid = downcast<RenderGrid>(child.parent());
    LayoutUnit mbp;

    while (grid->isSubgrid(direction)) {
        GridSpan span = grid->gridSpanForChild(child, direction);

        mbp += extraMarginForSubgrid(*grid, span.startLine(), span.endLine(), direction);

        const RenderElement* parent = grid->parent();
        if (!parent || !is<RenderGrid>(parent))
            break;

        const RenderGrid* parentGrid = downcast<RenderGrid>(parent);
        direction = flowAwareDirectionForParent(*grid, *parentGrid, direction);

        grid = parentGrid;
    }

    return mbp;
}

LayoutUnit marginLogicalSizeForChild(const RenderGrid& grid, GridTrackSizingDirection direction, const RenderBox& child)
{
    LayoutUnit margin;
    if (child.needsLayout())
        margin = computeMarginLogicalSizeForChild(grid, direction, child);
    else {
        GridTrackSizingDirection flowAwareDirection = flowAwareDirectionForChild(grid, child, direction);
        bool isRowAxis = flowAwareDirection == ForColumns;
        LayoutUnit marginStart = marginStartIsAuto(child, flowAwareDirection) ? 0_lu : isRowAxis ? child.marginStart() : child.marginBefore();
        LayoutUnit marginEnd = marginEndIsAuto(child, flowAwareDirection) ? 0_lu : isRowAxis ? child.marginEnd() : child.marginAfter();
        margin = marginStart + marginEnd;
    }

    if (&grid != child.parent()) {
        GridTrackSizingDirection subgridDirection = flowAwareDirectionForChild(grid, *downcast<RenderGrid>(child.parent()), direction);
        margin += extraMarginForSubgridAncestors(subgridDirection, child);
    }

    return margin;
}

bool isOrthogonalChild(const RenderGrid& grid, const RenderBox& child)
{
    return child.isHorizontalWritingMode() != grid.isHorizontalWritingMode();
}

bool isOrthogonalParent(const RenderGrid& grid, const RenderElement& parent)
{
    return parent.isHorizontalWritingMode() != grid.isHorizontalWritingMode();
}

bool isAspectRatioBlockSizeDependentChild(const RenderBox& child)
{
    return (child.style().hasAspectRatio() || child.hasIntrinsicAspectRatio()) && (child.hasRelativeLogicalHeight() || child.hasStretchedLogicalHeight());
}

GridTrackSizingDirection flowAwareDirectionForChild(const RenderGrid& grid, const RenderBox& child, GridTrackSizingDirection direction)
{
    return !isOrthogonalChild(grid, child) ? direction : (direction == ForColumns ? ForRows : ForColumns);
}

GridTrackSizingDirection flowAwareDirectionForParent(const RenderGrid& grid, const RenderElement& parent, GridTrackSizingDirection direction)
{
    return isOrthogonalParent(grid, parent) ? (direction == ForColumns ? ForRows : ForColumns) : direction;
}

bool hasOverridingContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    return direction == ForColumns ? child.hasOverridingContainingBlockContentLogicalWidth() : child.hasOverridingContainingBlockContentLogicalHeight();
}

std::optional<LayoutUnit> overridingContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    return direction == ForColumns ? child.overridingContainingBlockContentLogicalWidth() : child.overridingContainingBlockContentLogicalHeight();
}

bool isFlippedDirection(const RenderGrid& grid, GridTrackSizingDirection direction)
{
    if (direction == ForColumns)
        return !grid.style().isLeftToRightDirection();
    return grid.style().isFlippedBlocksWritingMode();
}

bool isSubgridReversedDirection(const RenderGrid& grid, GridTrackSizingDirection outerDirection, const RenderGrid& subgrid)
{
    GridTrackSizingDirection childDirection = flowAwareDirectionForChild(grid, subgrid, outerDirection);
    ASSERT(subgrid.isSubgrid(childDirection));
    return isFlippedDirection(grid, outerDirection) != isFlippedDirection(subgrid, childDirection);
}

} // namespace GridLayoutFunctions

} // namespace WebCore
