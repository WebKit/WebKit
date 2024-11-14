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

LayoutUnit GridBaselineAlignment::logicalAscentForGridItem(const RenderBox& gridItem, GridAxis alignmentAxis, ItemPosition position) const
{
    auto hasOrthogonalAncestorSubgrids = [&] {
        for (auto& currentAncestorSubgrid : ancestorSubgridsOfGridItem(gridItem, GridTrackSizingDirection::ForRows)) {
            if (currentAncestorSubgrid.isHorizontalWritingMode() != currentAncestorSubgrid.parent()->isHorizontalWritingMode())
                return true;
        }
        return false;
    };

    ExtraMarginsFromSubgrids extraMarginsFromAncestorSubgrids;
    if (alignmentAxis == GridAxis::GridColumnAxis && !hasOrthogonalAncestorSubgrids())
        extraMarginsFromAncestorSubgrids = GridLayoutFunctions::extraMarginForSubgridAncestors(GridTrackSizingDirection::ForRows, gridItem);

    LayoutUnit ascent = ascentForGridItem(gridItem, alignmentAxis, position) + extraMarginsFromAncestorSubgrids.extraTrackStartMargin();
    return (isDescentBaselineForGridItem(gridItem, alignmentAxis) || position == ItemPosition::LastBaseline) ? descentForGridItem(gridItem, ascent, alignmentAxis, extraMarginsFromAncestorSubgrids) : ascent;
}

LayoutUnit GridBaselineAlignment::ascentForGridItem(const RenderBox& gridItem, GridAxis alignmentAxis, ItemPosition position) const
{
    static const LayoutUnit noValidBaseline = LayoutUnit(-1);

    ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
    auto baseline = 0_lu;
    auto gridItemMargin = alignmentAxis == GridAxis::GridColumnAxis
        ? gridItem.marginBefore(m_writingMode)
        : gridItem.marginStart(m_writingMode);
    auto& parentStyle = gridItem.parent()->style();

    if (alignmentAxis == GridAxis::GridColumnAxis) {
        auto alignmentContextDirection = [&] {
            return parentStyle.writingMode().isHorizontal() ? LineDirectionMode::HorizontalLine : LineDirectionMode::VerticalLine;
        };

        if (!isParallelToAlignmentAxisForGridItem(gridItem, alignmentAxis))
            return gridItemMargin + synthesizedBaseline(gridItem, parentStyle, alignmentContextDirection(), BaselineSynthesisEdge::BorderBox);
        auto ascent = position == ItemPosition::Baseline ? gridItem.firstLineBaseline() : gridItem.lastLineBaseline();
        if (!ascent)
            return gridItemMargin + synthesizedBaseline(gridItem, parentStyle, alignmentContextDirection(), BaselineSynthesisEdge::BorderBox);
        baseline = ascent.value();
    } else {
        auto computedBaselineValue = position == ItemPosition::Baseline ? gridItem.firstLineBaseline() : gridItem.lastLineBaseline();
        baseline = isParallelToAlignmentAxisForGridItem(gridItem, alignmentAxis) ? computedBaselineValue.value_or(noValidBaseline) : noValidBaseline;
        // We take border-box's under edge if no valid baseline.
        if (baseline == noValidBaseline) {
            ASSERT(!gridItem.needsLayout());
            if (isVerticalAlignmentContext(alignmentAxis))
                return m_writingMode.isBlockFlipped() ? gridItemMargin + gridItem.size().width().toInt() : gridItemMargin;
            return gridItemMargin + synthesizedBaseline(gridItem, parentStyle, LineDirectionMode::HorizontalLine, BaselineSynthesisEdge::BorderBox);
        }
    }

    return gridItemMargin + baseline;
}

LayoutUnit GridBaselineAlignment::descentForGridItem(const RenderBox& gridItem, LayoutUnit ascent, GridAxis alignmentAxis, ExtraMarginsFromSubgrids extraMarginsFromAncestorSubgrids) const
{
    ASSERT(!gridItem.needsLayout());
    if (isParallelToAlignmentAxisForGridItem(gridItem, alignmentAxis))
        return extraMarginsFromAncestorSubgrids.extraTotalMargin() + gridItem.marginLogicalHeight() + gridItem.logicalHeight() - ascent;
    return gridItem.marginLogicalWidth() + gridItem.logicalWidth() - ascent;
}

bool GridBaselineAlignment::isDescentBaselineForGridItem(const RenderBox& gridItem, GridAxis alignmentAxis) const
{
    return isVerticalAlignmentContext(alignmentAxis)
        && ((gridItem.writingMode().isBlockFlipped() && !m_writingMode.isBlockFlipped())
            || (gridItem.writingMode().isLineInverted() && m_writingMode.isBlockFlipped()));
}

bool GridBaselineAlignment::isVerticalAlignmentContext(GridAxis alignmentAxis) const
{
    return (alignmentAxis == GridAxis::GridRowAxis) == m_writingMode.isHorizontal();
}

bool GridBaselineAlignment::isOrthogonalGridItemForBaseline(const RenderBox& gridItem) const
{
    return m_writingMode.isOrthogonal(gridItem.writingMode());
}

bool GridBaselineAlignment::isParallelToAlignmentAxisForGridItem(const RenderBox& gridItem, GridAxis alignmentAxis) const
{
    return alignmentAxis == GridAxis::GridColumnAxis ? !isOrthogonalGridItemForBaseline(gridItem) : isOrthogonalGridItemForBaseline(gridItem);
}

const BaselineGroup& GridBaselineAlignment::baselineGroupForGridItem(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, GridAxis alignmentAxis) const
{
    ASSERT(isBaselinePosition(preference));
    bool isRowAxisContext = alignmentAxis == GridAxis::GridColumnAxis;
    auto& baselineAlignmentStateMap = isRowAxisContext ? m_rowAxisBaselineAlignmentStates : m_colAxisBaselineAlignmentStates;
    auto* baselineAlignmentState = baselineAlignmentStateMap.get(sharedContext);
    ASSERT(baselineAlignmentState);
    return baselineAlignmentState->sharedGroup(gridItem, preference);
}

void GridBaselineAlignment::updateBaselineAlignmentContext(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, GridAxis alignmentAxis)
{
    ASSERT(isBaselinePosition(preference));
    ASSERT(!gridItem.needsLayout());

    // Determine Ascent and Descent values of this grid item with respect to
    // its grid container.
    LayoutUnit ascent = logicalAscentForGridItem(gridItem, alignmentAxis, preference);
    // Looking up for a shared alignment context perpendicular to the
    // alignment axis.
    bool isRowAxisContext = alignmentAxis == GridAxis::GridColumnAxis;
    auto& baselineAlignmentStateMap = isRowAxisContext ? m_rowAxisBaselineAlignmentStates : m_colAxisBaselineAlignmentStates;
    // Looking for a compatible baseline-sharing group.
    if (auto* baselineAlignmentStateSearch = baselineAlignmentStateMap.get(sharedContext))
        baselineAlignmentStateSearch->updateSharedGroup(gridItem, preference, ascent);
    else
        baselineAlignmentStateMap.add(sharedContext, makeUnique<BaselineAlignmentState>(gridItem, preference, ascent));
}

LayoutUnit GridBaselineAlignment::baselineOffsetForGridItem(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, GridAxis alignmentAxis) const
{
    ASSERT(isBaselinePosition(preference));
    auto& group = baselineGroupForGridItem(preference, sharedContext, gridItem, alignmentAxis);
    if (group.computeSize() > 1)
        return group.maxAscent() - logicalAscentForGridItem(gridItem, alignmentAxis, preference);
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
