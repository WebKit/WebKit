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

LayoutUnit GridBaselineAlignment::logicalAscentForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType, ItemPosition position) const
{
    auto hasOrthogonalAncestorSubgrids = [&] {
        for (auto& currentAncestorSubgrid : ancestorSubgridsOfGridItem(gridItem, Style::GridTrackSizingDirection::Rows)) {
            if (currentAncestorSubgrid.isHorizontalWritingMode() != currentAncestorSubgrid.parent()->isHorizontalWritingMode())
                return true;
        }
        return false;
    };

    ExtraMarginsFromSubgrids extraMarginsFromAncestorSubgrids;
    if (alignmentContextType == Style::GridTrackSizingDirection::Rows && !hasOrthogonalAncestorSubgrids())
        extraMarginsFromAncestorSubgrids = GridLayoutFunctions::extraMarginForSubgridAncestors(Style::GridTrackSizingDirection::Rows, gridItem);

    LayoutUnit ascent = ascentForGridItem(gridItem, alignmentContextType, position) + extraMarginsFromAncestorSubgrids.extraTrackStartMargin();
    return (isDescentBaselineForGridItem(gridItem, alignmentContextType) || position == ItemPosition::LastBaseline) ? descentForGridItem(gridItem, ascent, alignmentContextType, extraMarginsFromAncestorSubgrids) : ascent;
}

LayoutUnit GridBaselineAlignment::ascentForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType, ItemPosition position) const
{
    static const LayoutUnit noValidBaseline = LayoutUnit(-1);

    ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
    auto baseline = 0_lu;
    auto gridItemMargin = alignmentContextType == Style::GridTrackSizingDirection::Rows
        ? gridItem.marginBefore(m_writingMode)
        : gridItem.marginStart(m_writingMode);
    auto& gridStyle = gridItem.parent()->style();

    if (alignmentContextType == Style::GridTrackSizingDirection::Rows) {
        auto alignmentContextDirection = [&] {
            return gridStyle.writingMode().isHorizontal() ? LineDirection::Horizontal : LineDirection::Vertical;
        };

        if (!isParallelToAlignmentAxisForGridItem(gridItem, alignmentContextType)) {
            auto gridWritingMode = gridStyle.writingMode();
            return gridItemMargin + BaselineAlignmentState::synthesizedBaseline(gridItem, BaselineAlignmentState::dominantBaseline(gridWritingMode),
                gridWritingMode, alignmentContextDirection(), BaselineSynthesisEdge::BorderBox);
        }
        auto ascent = position == ItemPosition::Baseline ? gridItem.firstLineBaseline() : gridItem.lastLineBaseline();
        if (!ascent) {
            auto gridWritingMode = gridStyle.writingMode();
            return gridItemMargin + BaselineAlignmentState::synthesizedBaseline(gridItem, BaselineAlignmentState::dominantBaseline(gridWritingMode),
                gridWritingMode, alignmentContextDirection(), BaselineSynthesisEdge::BorderBox);
        }
        baseline = ascent.value();
    } else {
        auto computedBaselineValue = position == ItemPosition::Baseline ? gridItem.firstLineBaseline() : gridItem.lastLineBaseline();
        baseline = isParallelToAlignmentAxisForGridItem(gridItem, alignmentContextType) ? computedBaselineValue.value_or(noValidBaseline) : noValidBaseline;
        // We take border-box's under edge if no valid baseline.
        if (baseline == noValidBaseline) {
            ASSERT(!gridItem.needsLayout());
            if (isVerticalAlignmentContext(alignmentContextType))
                return m_writingMode.isBlockFlipped() ? gridItemMargin + gridItem.size().width().toInt() : gridItemMargin;
            auto gridWritingMode = gridStyle.writingMode();
            return gridItemMargin + BaselineAlignmentState::synthesizedBaseline(gridItem, BaselineAlignmentState::dominantBaseline(gridWritingMode),
                gridWritingMode, LineDirection::Horizontal, BaselineSynthesisEdge::BorderBox);
        }
    }

    return gridItemMargin + baseline;
}

LayoutUnit GridBaselineAlignment::descentForGridItem(const RenderBox& gridItem, LayoutUnit ascent, Style::GridTrackSizingDirection alignmentContextType, ExtraMarginsFromSubgrids extraMarginsFromAncestorSubgrids) const
{
    ASSERT(!gridItem.needsLayout());
    if (isParallelToAlignmentAxisForGridItem(gridItem, alignmentContextType))
        return extraMarginsFromAncestorSubgrids.extraTotalMargin() + gridItem.marginLogicalHeight() + gridItem.logicalHeight() - ascent;
    return gridItem.marginLogicalWidth() + gridItem.logicalWidth() - ascent;
}

bool GridBaselineAlignment::isDescentBaselineForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType) const
{
    return isVerticalAlignmentContext(alignmentContextType)
        && ((gridItem.writingMode().isBlockFlipped() && !m_writingMode.isBlockFlipped())
            || (gridItem.writingMode().isLineInverted() && m_writingMode.isBlockFlipped()));
}

bool GridBaselineAlignment::isVerticalAlignmentContext(Style::GridTrackSizingDirection alignmentContextType) const
{
    return (alignmentContextType == Style::GridTrackSizingDirection::Columns) == m_writingMode.isHorizontal();
}

bool GridBaselineAlignment::isOrthogonalGridItemForBaseline(const RenderBox& gridItem) const
{
    return m_writingMode.isOrthogonal(gridItem.writingMode());
}

bool GridBaselineAlignment::isParallelToAlignmentAxisForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType) const
{
    return alignmentContextType == Style::GridTrackSizingDirection::Rows ? !isOrthogonalGridItemForBaseline(gridItem) : isOrthogonalGridItemForBaseline(gridItem);
}

const BaselineGroup& GridBaselineAlignment::baselineGroupForGridItem(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType) const
{
    ASSERT(isBaselinePosition(preference));
    auto& baselineAlignmentStateMap = alignmentContextType == Style::GridTrackSizingDirection::Rows ? m_rowAlignmentContextStates : m_columnAlignmentContextStates;
    auto* baselineAlignmentState = baselineAlignmentStateMap.get(sharedContext);
    ASSERT(baselineAlignmentState);
    return baselineAlignmentState->sharedGroup(gridItem, preference);
}

void GridBaselineAlignment::updateBaselineAlignmentContext(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType)
{
    ASSERT(isBaselinePosition(preference));
    ASSERT(!gridItem.needsLayout());

    // Determine Ascent and Descent values of this grid item with respect to
    // its grid container.
    LayoutUnit ascent = logicalAscentForGridItem(gridItem, alignmentContextType, preference);
    // Looking up for a shared alignment context perpendicular to the
    // alignment axis.
    auto& baselineAlignmentStateMap = alignmentContextType == Style::GridTrackSizingDirection::Rows ? m_rowAlignmentContextStates : m_columnAlignmentContextStates;
    // Looking for a compatible baseline-sharing group.
    baselineAlignmentStateMap.ensure(sharedContext, [&] {
        auto alignmentAxis = alignmentContextType == Style::GridTrackSizingDirection::Columns ? LogicalBoxAxis::Block : LogicalBoxAxis::Inline;
        return makeUnique<BaselineAlignmentState>(gridItem, preference, ascent, alignmentAxis, m_writingMode);
    }).iterator->value->updateSharedGroup(gridItem, preference, ascent);
}

LayoutUnit GridBaselineAlignment::baselineOffsetForGridItem(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType) const
{
    ASSERT(isBaselinePosition(preference));
    auto& group = baselineGroupForGridItem(preference, sharedContext, gridItem, alignmentContextType);
    if (group.computeSize() > 1)
        return group.maxAscent() - logicalAscentForGridItem(gridItem, alignmentContextType, preference);
    return LayoutUnit();
}

void GridBaselineAlignment::clear(Style::GridTrackSizingDirection alignmentContextType)
{
    if (alignmentContextType == Style::GridTrackSizingDirection::Rows)
        m_rowAlignmentContextStates.clear();
    else
        m_columnAlignmentContextStates.clear();
}

} // namespace WebCore
