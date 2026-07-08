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
        for (CheckedRef currentAncestorSubgrid : ancestorSubgridsOfGridItem(gridItem, Style::GridTrackSizingDirection::Rows)) {
            if (currentAncestorSubgrid->isHorizontalWritingMode() != currentAncestorSubgrid->parent()->isHorizontalWritingMode())
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
    ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
    auto gridItemMargin = alignmentContextType == Style::GridTrackSizingDirection::Rows ? gridItem.marginBefore(m_writingMode) : gridItem.marginStart(m_writingMode);
    CheckedRef gridStyle = gridItem.parent()->style();

    auto baseline = 0_lu;
    if (alignmentContextType == Style::GridTrackSizingDirection::Rows) {
        auto alignmentContextDirection = [&] {
            return gridStyle->writingMode().isHorizontal() ? LineDirection::Horizontal : LineDirection::Vertical;
        };

        if (!isParallelToAlignmentAxisForGridItem(gridItem, alignmentContextType)) {
            auto gridWritingMode = gridStyle->writingMode();
            return gridItemMargin + BaselineAlignment::synthesizedBaseline(gridItem, BaselineAlignment::dominantBaseline(gridWritingMode),
                gridWritingMode, alignmentContextDirection(), BaselineSynthesisEdge::BorderBox);
        }
        auto ascent = position == ItemPosition::Baseline ? gridItem.firstLineBaseline() : gridItem.lastLineBaseline();
        if (!ascent) {
            auto gridWritingMode = gridStyle->writingMode();
            return gridItemMargin + BaselineAlignment::synthesizedBaseline(gridItem, BaselineAlignment::dominantBaseline(gridWritingMode),
                gridWritingMode, alignmentContextDirection(), BaselineSynthesisEdge::BorderBox);
        }
        baseline = *ascent;
    } else {
        auto firstOrLastLineBaseline = std::optional<LayoutUnit> { };
        if (isParallelToAlignmentAxisForGridItem(gridItem, alignmentContextType))
            firstOrLastLineBaseline = position == ItemPosition::Baseline ? gridItem.firstLineBaseline() : gridItem.lastLineBaseline();

        // We take border-box's under edge if no valid baseline.
        if (!firstOrLastLineBaseline) {
            ASSERT(!gridItem.needsLayout());
            if (isVerticalAlignmentContext(alignmentContextType))
                return m_writingMode.isBlockFlipped() ? gridItemMargin + gridItem.borderBoxSize().width().toInt() : gridItemMargin;
            auto gridWritingMode = gridStyle->writingMode();
            return gridItemMargin + BaselineAlignment::synthesizedBaseline(gridItem, BaselineAlignment::dominantBaseline(gridWritingMode),
                gridWritingMode, LineDirection::Horizontal, BaselineSynthesisEdge::BorderBox);
        }
        baseline = *firstOrLastLineBaseline;
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

void GridBaselineAlignment::updateBaselineAlignmentContext(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType)
{
    ASSERT(isBaselinePosition(preference));
    ASSERT(!gridItem.needsLayout());

    // Determine this grid item's ascent with respect to its grid container.
    LayoutUnit ascent = logicalAscentForGridItem(gridItem, alignmentContextType, preference);
    // Find (or create) the baseline alignment-context perpendicular to the alignment axis, then fold this
    // item's ascent into the max ascent of the baseline-sharing group it belongs to.
    auto& contextMap = alignmentContextType == Style::GridTrackSizingDirection::Rows ? m_rowAlignmentContextStates : m_columnAlignmentContextStates;
    auto& context = contextMap.ensure(sharedContext, [&] {
        auto alignmentAxis = alignmentContextType == Style::GridTrackSizingDirection::Columns ? LogicalBoxAxis::Block : LogicalBoxAxis::Inline;
        return AlignmentContext { makeUnique<BaselineAlignmentState>(alignmentAxis, m_writingMode), { } };
    }).iterator->value;
    auto groupIndex = context.sharedGroups->sharedGroupIndex(gridItem.writingMode(), preference);
    if (groupIndex == context.maxAscents.size())
        context.maxAscents.append(LayoutUnit());
    context.maxAscents[groupIndex] = std::max(context.maxAscents[groupIndex], ascent);
}

LayoutUnit GridBaselineAlignment::baselineOffsetForGridItem(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContextType) const
{
    ASSERT(isBaselinePosition(preference));
    auto& contextMap = alignmentContextType == Style::GridTrackSizingDirection::Rows ? m_rowAlignmentContextStates : m_columnAlignmentContextStates;
    auto it = contextMap.find(sharedContext);
    ASSERT(it != contextMap.end());
    auto& context = it->value;
    auto groupIndex = context.sharedGroups->sharedGroupIndex(gridItem.writingMode(), preference);
    // No recorded ascent means a lone participant (its own ascent is the max), so there is no baseline shim.
    if (groupIndex >= context.maxAscents.size())
        return { };
    return context.maxAscents[groupIndex] - logicalAscentForGridItem(gridItem, alignmentContextType, preference);
}

void GridBaselineAlignment::clear(Style::GridTrackSizingDirection alignmentContextType)
{
    if (alignmentContextType == Style::GridTrackSizingDirection::Rows)
        m_rowAlignmentContextStates.clear();
    else
        m_columnAlignmentContextStates.clear();
}

} // namespace WebCore
