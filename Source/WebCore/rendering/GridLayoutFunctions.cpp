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
    if (!childHasMargin(child, flowAwareDirectionForChild(grid, child, direction)))
        return 0;

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (direction == ForColumns)
        child.computeInlineDirectionMargins(grid, child.containingBlockLogicalWidthForContentInFragment(nullptr), child.logicalWidth(), marginStart, marginEnd);
    else
        child.computeBlockDirectionMargins(grid, marginStart, marginEnd);
    return marginStartIsAuto(child, direction) ? marginEnd : marginEndIsAuto(child, direction) ? marginStart : marginStart + marginEnd;
}

LayoutUnit marginLogicalSizeForChild(const RenderGrid& grid, GridTrackSizingDirection direction, const RenderBox& child)
{
    if (child.needsLayout())
        return computeMarginLogicalSizeForChild(grid, direction, child);
    bool isRowAxis = flowAwareDirectionForChild(grid, child, direction) == ForColumns;
    LayoutUnit marginStart = marginStartIsAuto(child, direction) ? 0_lu : isRowAxis ? child.marginStart() : child.marginBefore();
    LayoutUnit marginEnd = marginEndIsAuto(child, direction) ? 0_lu : isRowAxis ? child.marginEnd() : child.marginAfter();
    return marginStart + marginEnd;
}

bool isOrthogonalChild(const RenderGrid& grid, const RenderBox& child)
{
    return child.isHorizontalWritingMode() != grid.isHorizontalWritingMode();
}

GridTrackSizingDirection flowAwareDirectionForChild(const RenderGrid& grid, const RenderBox& child, GridTrackSizingDirection direction)
{
    return !isOrthogonalChild(grid, child) ? direction : (direction == ForColumns ? ForRows : ForColumns);
}

bool hasOverrideContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    return direction == ForColumns ? child.hasOverrideContainingBlockContentLogicalWidth() : child.hasOverrideContainingBlockContentLogicalHeight();
}

Optional<LayoutUnit> overrideContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    return direction == ForColumns ? child.overrideContainingBlockContentLogicalWidth() : child.overrideContainingBlockContentLogicalHeight();
}

} // namespace GridLayoutFunctions

} // namespace WebCore
