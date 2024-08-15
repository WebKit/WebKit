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

#include "AncestorSubgridIterator.h"
#include "LengthFunctions.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderGrid.h"
#include "RenderStyleConstants.h"
#include "RenderStyleInlines.h"

namespace WebCore {

namespace GridLayoutFunctions {

static inline bool marginStartIsAuto(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    return direction == GridTrackSizingDirection::ForColumns ? gridItem.style().marginStart().isAuto() : gridItem.style().marginBefore().isAuto();
}

static inline bool marginEndIsAuto(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    return direction == GridTrackSizingDirection::ForColumns ? gridItem.style().marginEnd().isAuto() : gridItem.style().marginAfter().isAuto();
}

static bool gridItemHasMargin(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    // Length::IsZero returns true for 'auto' margins, which is aligned with the purpose of this function.
    if (direction == GridTrackSizingDirection::ForColumns)
        return !gridItem.style().marginStart().isZero() || !gridItem.style().marginEnd().isZero();
    return !gridItem.style().marginBefore().isZero() || !gridItem.style().marginAfter().isZero();
}

LayoutUnit computeMarginLogicalSizeForGridItem(const RenderGrid& grid, GridTrackSizingDirection direction, const RenderBox& gridItem)
{
    GridTrackSizingDirection flowAwareDirection = flowAwareDirectionForGridItem(grid, gridItem, direction);
    if (!gridItemHasMargin(gridItem, flowAwareDirection))
        return 0;

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (direction == GridTrackSizingDirection::ForColumns)
        gridItem.computeInlineDirectionMargins(grid, gridItem.containingBlockLogicalWidthForContentInFragment(nullptr), { }, gridItem.logicalWidth(), marginStart, marginEnd);
    else
        gridItem.computeBlockDirectionMargins(grid, marginStart, marginEnd);
    return marginStartIsAuto(gridItem, flowAwareDirection) ? marginEnd : marginEndIsAuto(gridItem, flowAwareDirection) ? marginStart : marginStart + marginEnd;
}

bool hasRelativeOrIntrinsicSizeForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    if (direction == GridTrackSizingDirection::ForColumns)
        return gridItem.hasRelativeLogicalWidth() || gridItem.style().logicalWidth().isIntrinsicOrAuto();
    return gridItem.hasRelativeLogicalHeight() || gridItem.style().logicalHeight().isIntrinsicOrAuto();
}

static ExtraMarginsFromSubgrids extraMarginForSubgrid(const RenderGrid& parent, unsigned startLine, unsigned endLine, GridTrackSizingDirection direction)
{
    unsigned numTracks = parent.numTracks(direction);
    if (!numTracks || !parent.isSubgrid(direction))
        return { };

    std::optional<LayoutUnit> availableSpace;
    if (!hasRelativeOrIntrinsicSizeForGridItem(parent, direction))
        availableSpace = parent.availableSpaceForGutters(direction);

    RenderGrid& grandParent = downcast<RenderGrid>(*parent.parent());
    ExtraMarginsFromSubgrids extraMargins;
    if (!startLine)
        extraMargins.addTrackStartMargin((direction == GridTrackSizingDirection::ForColumns) ? parent.marginAndBorderAndPaddingStart() : parent.marginAndBorderAndPaddingBefore());
    else
        extraMargins.addTrackStartMargin((parent.gridGap(direction, availableSpace) - grandParent.gridGap(direction)) / 2);

    if (endLine == numTracks)
        extraMargins.addTrackEndMargin((direction == GridTrackSizingDirection::ForColumns) ? parent.marginAndBorderAndPaddingEnd() : parent.marginAndBorderAndPaddingAfter());
    else
        extraMargins.addTrackEndMargin((parent.gridGap(direction, availableSpace) - grandParent.gridGap(direction)) / 2);

    return extraMargins;
}

ExtraMarginsFromSubgrids extraMarginForSubgridAncestors(GridTrackSizingDirection direction, const RenderBox& gridItem)
{
    ExtraMarginsFromSubgrids extraMargins;
    for (auto& currentAncestorSubgrid : ancestorSubgridsOfGridItem(gridItem, direction)) {
        GridSpan span = currentAncestorSubgrid.gridSpanForGridItem(gridItem, direction);
        extraMargins += extraMarginForSubgrid(currentAncestorSubgrid, span.startLine(), span.endLine(), direction);
    }
    return extraMargins;
}

LayoutUnit marginLogicalSizeForGridItem(const RenderGrid& grid, GridTrackSizingDirection direction, const RenderBox& gridItem)
{
    auto margin = computeMarginLogicalSizeForGridItem(grid, direction, gridItem);

    if (&grid != gridItem.parent()) {
        GridTrackSizingDirection subgridDirection = flowAwareDirectionForGridItem(grid, *downcast<RenderGrid>(gridItem.parent()), direction);
        margin += extraMarginForSubgridAncestors(subgridDirection, gridItem).extraTotalMargin();
    }

    return margin;
}

bool isOrthogonalGridItem(const RenderGrid& grid, const RenderBox& gridItem)
{
    return gridItem.isHorizontalWritingMode() != grid.isHorizontalWritingMode();
}

bool isOrthogonalParent(const RenderGrid& grid, const RenderElement& parent)
{
    return parent.isHorizontalWritingMode() != grid.isHorizontalWritingMode();
}

bool isAspectRatioBlockSizeDependentGridItem(const RenderBox& gridItem)
{
    return (gridItem.style().hasAspectRatio() || gridItem.hasIntrinsicAspectRatio()) && (gridItem.hasRelativeLogicalHeight() || gridItem.hasStretchedLogicalHeight());
}

bool isGridItemInlineSizeDependentOnBlockConstraints(const RenderBox& gridItem, const RenderGrid& parentGrid, ItemPosition gridItemAlignSelf)
{
    ASSERT(gridItem.parent() == &parentGrid);

    if (isOrthogonalGridItem(parentGrid, gridItem))
        return true;

    auto& gridItemStyle = gridItem.style();
    auto gridItemFlexWrap = gridItemStyle.flexWrap();
    if (gridItem.isRenderFlexibleBox() && gridItem.style().isColumnFlexDirection() && (gridItemFlexWrap == FlexWrap::Wrap || gridItemFlexWrap == FlexWrap::Reverse))
        return true;

    if (gridItem.isRenderMultiColumnFlow())
        return true;

    if (isAspectRatioBlockSizeDependentGridItem(gridItem))
        return true;


    auto hasAspectRatioAndInlineSizeDependsOnBlockSize = [](const RenderObject& renderer) {
        auto& rendererStyle = renderer.style();
        bool rendererHasAspectRatio = renderer.hasIntrinsicAspectRatio() || rendererStyle.hasAspectRatio();

        return rendererHasAspectRatio && rendererStyle.logicalWidth().isAuto() && !rendererStyle.logicalHeight().isIntrinsicOrAuto();
    };

    // Stretch alignment allows the grid item content to resolve against the stretched size.
    if (gridItemAlignSelf != ItemPosition::Stretch)
        return false;

    for (auto& gridItemChild : childrenOfType<RenderObject>(gridItem)) {
        if (hasAspectRatioAndInlineSizeDependsOnBlockSize(gridItemChild))
            return true;
    }

    return false;
}

GridTrackSizingDirection flowAwareDirectionForGridItem(const RenderGrid& grid, const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    return !isOrthogonalGridItem(grid, gridItem) ? direction : (direction == GridTrackSizingDirection::ForColumns ? GridTrackSizingDirection::ForRows : GridTrackSizingDirection::ForColumns);
}

GridTrackSizingDirection flowAwareDirectionForParent(const RenderGrid& grid, const RenderElement& parent, GridTrackSizingDirection direction)
{
    return isOrthogonalParent(grid, parent) ? (direction == GridTrackSizingDirection::ForColumns ? GridTrackSizingDirection::ForRows : GridTrackSizingDirection::ForColumns) : direction;
}

std::optional<RenderBox::ContainingBlockOverrideValue> overridingContainingBlockContentSizeForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    return direction == GridTrackSizingDirection::ForColumns ? gridItem.overridingContainingBlockContentLogicalWidth() : gridItem.overridingContainingBlockContentLogicalHeight();
}

bool isFlippedDirection(const RenderGrid& grid, GridTrackSizingDirection direction)
{
    if (direction == GridTrackSizingDirection::ForColumns)
        return !grid.style().isLeftToRightDirection();
    return grid.style().isFlippedBlocksWritingMode();
}

bool isSubgridReversedDirection(const RenderGrid& grid, GridTrackSizingDirection outerDirection, const RenderGrid& subgrid)
{
    GridTrackSizingDirection subgridDirection = flowAwareDirectionForGridItem(grid, subgrid, outerDirection);
    ASSERT(subgrid.isSubgrid(subgridDirection));
    return isFlippedDirection(grid, outerDirection) != isFlippedDirection(subgrid, subgridDirection);
}

unsigned alignmentContextForBaselineAlignment(const GridSpan& span, const ItemPosition& alignment)
{
    ASSERT(alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline);
    if (alignment == ItemPosition::Baseline)
        return span.startLine();
    return span.endLine() - 1;
}

void setOverridingContentSizeForGridItem(const RenderGrid& renderGrid, RenderBox& gridItem, LayoutUnit logicalSize, GridTrackSizingDirection direction)
{
    if (!isOrthogonalGridItem(renderGrid, gridItem))
        direction == GridTrackSizingDirection::ForColumns ? gridItem.setOverridingLogicalWidth(logicalSize) : gridItem.setOverridingLogicalHeight(logicalSize);
    else
        direction == GridTrackSizingDirection::ForColumns ? gridItem.setOverridingLogicalHeight(logicalSize) : gridItem.setOverridingLogicalWidth(logicalSize);
}

void clearOverridingContentSizeForGridItem(const RenderGrid& renderGrid, RenderBox &gridItem, GridTrackSizingDirection direction)
{
    if (!isOrthogonalGridItem(renderGrid, gridItem))
        direction == GridTrackSizingDirection::ForColumns ? gridItem.clearOverridingLogicalWidth() : gridItem.clearOverridingLogicalHeight();
    else
        direction == GridTrackSizingDirection::ForColumns ? gridItem.clearOverridingLogicalHeight() : gridItem.clearOverridingLogicalWidth();
}

} // namespace GridLayoutFunctions

} // namespace WebCore
