/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GridBaselineAlignment.h"

#include "AncestorSubgridIterator.h"
#include "BaselineAlignmentInlines.h"
#include "RenderBoxInlines.h"
#include "RenderGrid.h"
#include "RenderStyleConstants.h"

namespace WebCore {

// This function gives the margin 'over' based on the baseline-axis, since in grid we can have 2-dimensional
// alignment by baseline. In horizontal writing-mode, the row-axis is the horizontal axis. When we use this
// axis to move the grid items so that they are baseline-aligned, we want their "horizontal" margin (right);
// the same will happen when using the column-axis under vertical writing mode, we also want in this case the
// 'right' margin.
LayoutUnit GridBaselineAlignment::marginOverForChild(const RenderBox& child, GridAxis alignmentAxis) const
{
    return isVerticalAlignmentContext(alignmentAxis) ? child.marginRight() : child.marginTop();
}

// This function gives the margin 'under' based on the baseline-axis, since in grid we can can 2-dimensional
// alignment by baseline. In horizontal writing-mode, the row-axis is the horizontal axis. When we use this
// axis to move the grid items so that they are baseline-aligned, we want their "horizontal" margin (left);
// the same will happen when using the column-axis under vertical writing mode, we also want in this case the
// 'left' margin.
LayoutUnit GridBaselineAlignment::marginUnderForChild(const RenderBox& child, GridAxis alignmentAxis) const
{
    return isVerticalAlignmentContext(alignmentAxis) ? child.marginLeft() : child.marginBottom();
}

LayoutUnit GridBaselineAlignment::logicalAscentForChild(const RenderBox& child, GridAxis alignmentAxis, ItemPosition position) const
{
    auto hasOrthogonalAncestorSubgrids = [&] {
        for (auto& currentAncestorSubgrid : ancestorSubgridsOfGridItem(child, GridTrackSizingDirection::ForRows)) {
            if (currentAncestorSubgrid.isHorizontalWritingMode() != currentAncestorSubgrid.parent()->isHorizontalWritingMode())
                return true;
        }
        return false;
    };

    ExtraMarginsFromSubgrids extraMarginsFromAncestorSubgrids;
    if (alignmentAxis == GridAxis::GridColumnAxis && !hasOrthogonalAncestorSubgrids())
        extraMarginsFromAncestorSubgrids = GridLayoutFunctions::extraMarginForSubgridAncestors(GridTrackSizingDirection::ForRows, child);

    LayoutUnit ascent = ascentForChild(child, alignmentAxis, position) + extraMarginsFromAncestorSubgrids.extraTrackStartMargin();
    return (isDescentBaselineForChild(child, alignmentAxis) || position == ItemPosition::LastBaseline) ? descentForChild(child, ascent, alignmentAxis, extraMarginsFromAncestorSubgrids) : ascent;
}

LayoutUnit GridBaselineAlignment::ascentForChild(const RenderBox& child, GridAxis alignmentAxis, ItemPosition position) const
{
    static const LayoutUnit noValidBaseline = LayoutUnit(-1);

    ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
    auto baseline = 0_lu;
    LayoutUnit margin = isDescentBaselineForChild(child, alignmentAxis) ? marginUnderForChild(child, alignmentAxis) : marginOverForChild(child, alignmentAxis);

    if (alignmentAxis == GridAxis::GridColumnAxis) {
        ASSERT(child.parentStyle());
        auto* parentStyle = child.parentStyle();

        auto alignmentContextDirection = [&] {
            return child.parentStyle()->isHorizontalWritingMode() ? LineDirectionMode::HorizontalLine : LineDirectionMode::VerticalLine;
        };

        if (!isParallelToAlignmentAxisForChild(child, alignmentAxis))
            return (parentStyle ? synthesizedBaseline(child, *child.parentStyle(), alignmentContextDirection(), BaselineSynthesisEdge::BorderBox) : 0_lu) + margin;
        auto ascent = position == ItemPosition::Baseline ? child.firstLineBaseline() : child.lastLineBaseline();
        if (!ascent)
            return (parentStyle ? synthesizedBaseline(child, *child.parentStyle(), alignmentContextDirection(), BaselineSynthesisEdge::BorderBox) : 0_lu) + margin;
        baseline = ascent.value();
    } else {
        auto computedBaselineValue = position == ItemPosition::Baseline ? child.firstLineBaseline() : child.lastLineBaseline();
        baseline = isParallelToAlignmentAxisForChild(child, alignmentAxis) ? computedBaselineValue.value_or(noValidBaseline) : noValidBaseline;
        // We take border-box's under edge if no valid baseline.
        if (baseline == noValidBaseline) {
            ASSERT(!child.needsLayout());
            if (isVerticalAlignmentContext(alignmentAxis))
                return isFlippedWritingMode(m_blockFlow) ? child.size().width().toInt() + margin : margin;
            return child.size().height() + margin;
        }
    }

    return margin + baseline;
}

LayoutUnit GridBaselineAlignment::descentForChild(const RenderBox& child, LayoutUnit ascent, GridAxis alignmentAxis, ExtraMarginsFromSubgrids extraMarginsFromAncestorSubgrids) const
{
    ASSERT(!child.needsLayout());
    if (isParallelToAlignmentAxisForChild(child, alignmentAxis))
        return extraMarginsFromAncestorSubgrids.extraTotalMargin() + child.marginLogicalHeight() + child.logicalHeight() - ascent;
    return child.marginLogicalWidth() + child.logicalWidth() - ascent;
}

bool GridBaselineAlignment::isDescentBaselineForChild(const RenderBox& child, GridAxis alignmentAxis) const
{
    return isVerticalAlignmentContext(alignmentAxis)
        && ((child.style().isFlippedBlocksWritingMode() && !isFlippedWritingMode(m_blockFlow))
            || (child.style().isFlippedLinesWritingMode() && isFlippedWritingMode(m_blockFlow)));
}

bool GridBaselineAlignment::isVerticalAlignmentContext(GridAxis alignmentAxis) const
{
    return alignmentAxis == GridAxis::GridRowAxis ? isHorizontalWritingMode(m_blockFlow) : !isHorizontalWritingMode(m_blockFlow);
}

bool GridBaselineAlignment::isOrthogonalChildForBaseline(const RenderBox& child) const
{
    return isHorizontalWritingMode(m_blockFlow) != child.isHorizontalWritingMode();
}

bool GridBaselineAlignment::isParallelToAlignmentAxisForChild(const RenderBox& child, GridAxis alignmentAxis) const
{
    return alignmentAxis == GridAxis::GridColumnAxis ? !isOrthogonalChildForBaseline(child) : isOrthogonalChildForBaseline(child);
}

const BaselineGroup& GridBaselineAlignment::baselineGroupForChild(ItemPosition preference, unsigned sharedContext, const RenderBox& child, GridAxis alignmentAxis) const
{
    ASSERT(isBaselinePosition(preference));
    bool isRowAxisContext = alignmentAxis == GridAxis::GridColumnAxis;
    auto& baselineAlignmentStateMap = isRowAxisContext ? m_rowAxisBaselineAlignmentStates : m_colAxisBaselineAlignmentStates;
    auto* baselineAlignmentState = baselineAlignmentStateMap.get(sharedContext);
    ASSERT(baselineAlignmentState);
    return baselineAlignmentState->sharedGroup(child, preference);
}

void GridBaselineAlignment::updateBaselineAlignmentContext(ItemPosition preference, unsigned sharedContext, const RenderBox& child, GridAxis alignmentAxis)
{
    ASSERT(isBaselinePosition(preference));
    ASSERT(!child.needsLayout());

    // Determine Ascent and Descent values of this child with respect to
    // its grid container.
    LayoutUnit ascent = logicalAscentForChild(child, alignmentAxis, preference);
    // Looking up for a shared alignment context perpendicular to the
    // alignment axis.
    bool isRowAxisContext = alignmentAxis == GridAxis::GridColumnAxis;
    auto& baselineAlignmentStateMap = isRowAxisContext ? m_rowAxisBaselineAlignmentStates : m_colAxisBaselineAlignmentStates;
    // Looking for a compatible baseline-sharing group.
    if (auto* baselineAlignmentStateSearch = baselineAlignmentStateMap.get(sharedContext))
        baselineAlignmentStateSearch->updateSharedGroup(child, preference, ascent);
    else
        baselineAlignmentStateMap.add(sharedContext, makeUnique<BaselineAlignmentState>(child, preference, ascent));
}

LayoutUnit GridBaselineAlignment::baselineOffsetForChild(ItemPosition preference, unsigned sharedContext, const RenderBox& child, GridAxis alignmentAxis) const
{
    ASSERT(isBaselinePosition(preference));
    auto& group = baselineGroupForChild(preference, sharedContext, child, alignmentAxis);
    if (group.computeSize() > 1)
        return group.maxAscent() - logicalAscentForChild(child, alignmentAxis, preference);
    return LayoutUnit();
}

void GridBaselineAlignment::clear(GridAxis alignmentAxis)
{
    if (alignmentAxis == GridAxis::GridColumnAxis)
        m_rowAxisBaselineAlignmentStates.clear();
    else
        m_colAxisBaselineAlignmentStates.clear();
}

} // namespace WebCore
