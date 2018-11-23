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

#include "RenderStyle.h"

namespace WebCore {

// This function gives the margin 'over' based on the baseline-axis, since in grid we can have 2-dimensional
// alignment by baseline. In horizontal writing-mode, the row-axis is the horizontal axis. When we use this
// axis to move the grid items so that they are baseline-aligned, we want their "horizontal" margin (right);
// the same will happen when using the column-axis under vertical writing mode, we also want in this case the
// 'right' margin.
LayoutUnit GridBaselineAlignment::marginOverForChild(const RenderBox& child, GridAxis axis) const
{
    return isHorizontalBaselineAxis(axis) ? child.marginRight() : child.marginTop();
}

// This function gives the margin 'under' based on the baseline-axis, since in grid we can can 2-dimensional
// alignment by baseline. In horizontal writing-mode, the row-axis is the horizontal axis. When we use this
// axis to move the grid items so that they are baseline-aligned, we want their "horizontal" margin (left);
// the same will happen when using the column-axis under vertical writing mode, we also want in this case the
// 'left' margin.
LayoutUnit GridBaselineAlignment::marginUnderForChild(const RenderBox& child, GridAxis axis) const
{
    return isHorizontalBaselineAxis(axis) ? child.marginLeft() : child.marginBottom();
}

LayoutUnit GridBaselineAlignment::logicalAscentForChild(const RenderBox& child, GridAxis baselineAxis) const
{
    LayoutUnit ascent = ascentForChild(child, baselineAxis);
    return isDescentBaselineForChild(child, baselineAxis) ? descentForChild(child, ascent, baselineAxis) : ascent;
}

LayoutUnit GridBaselineAlignment::ascentForChild(const RenderBox& child, GridAxis baselineAxis) const
{
    LayoutUnit margin = isDescentBaselineForChild(child, baselineAxis) ? marginUnderForChild(child, baselineAxis) : marginOverForChild(child, baselineAxis);
    LayoutUnit baseline(isParallelToBaselineAxisForChild(child, baselineAxis) ? child.firstLineBaseline().value_or(-1) : -1);
    // We take border-box's under edge if no valid baseline.
    if (baseline == -1) {
        if (isHorizontalBaselineAxis(baselineAxis))
            return isFlippedWritingMode(m_blockFlow) ? child.size().width().toInt() + margin : margin;
        return child.size().height() + margin;
    }
    return baseline + margin;
}

LayoutUnit GridBaselineAlignment::descentForChild(const RenderBox& child, LayoutUnit ascent, GridAxis baselineAxis) const
{
    if (isParallelToBaselineAxisForChild(child, baselineAxis))
        return child.marginLogicalHeight() + child.logicalHeight() - ascent;
    return child.marginLogicalWidth() + child.logicalWidth() - ascent;
}

bool GridBaselineAlignment::isDescentBaselineForChild(const RenderBox& child, GridAxis baselineAxis) const
{
    return isHorizontalBaselineAxis(baselineAxis)
        && ((child.style().isFlippedBlocksWritingMode() && !isFlippedWritingMode(m_blockFlow))
            || (child.style().isFlippedLinesWritingMode() && isFlippedWritingMode(m_blockFlow)));
}

bool GridBaselineAlignment::isHorizontalBaselineAxis(GridAxis axis) const
{
    return axis == GridRowAxis ? isHorizontalWritingMode(m_blockFlow) : !isHorizontalWritingMode(m_blockFlow);
}

bool GridBaselineAlignment::isOrthogonalChildForBaseline(const RenderBox& child) const
{
    return isHorizontalWritingMode(m_blockFlow) != child.isHorizontalWritingMode();
}

bool GridBaselineAlignment::isParallelToBaselineAxisForChild(const RenderBox& child, GridAxis axis) const
{
    return axis == GridColumnAxis ? !isOrthogonalChildForBaseline(child) : isOrthogonalChildForBaseline(child);
}

const BaselineGroup& GridBaselineAlignment::baselineGroupForChild(ItemPosition preference, unsigned sharedContext, const RenderBox& child, GridAxis baselineAxis) const
{
    ASSERT(isBaselinePosition(preference));
    bool isRowAxisContext = baselineAxis == GridColumnAxis;
    auto& contextsMap = isRowAxisContext ? m_rowAxisAlignmentContext : m_colAxisAlignmentContext;
    auto* context = contextsMap.get(sharedContext);
    ASSERT(context);
    return context->sharedGroup(child, preference);
}

void GridBaselineAlignment::updateBaselineAlignmentContext(ItemPosition preference, unsigned sharedContext, const RenderBox& child, GridAxis baselineAxis)
{
    ASSERT(isBaselinePosition(preference));
    ASSERT(!child.needsLayout());

    // Determine Ascent and Descent values of this child with respect to
    // its grid container.
    LayoutUnit ascent = ascentForChild(child, baselineAxis);
    LayoutUnit descent = descentForChild(child, ascent, baselineAxis);
    if (isDescentBaselineForChild(child, baselineAxis))
        std::swap(ascent, descent);

    // Looking up for a shared alignment context perpendicular to the
    // baseline axis.
    bool isRowAxisContext = baselineAxis == GridColumnAxis;
    auto& contextsMap = isRowAxisContext ? m_rowAxisAlignmentContext : m_colAxisAlignmentContext;
    auto addResult = contextsMap.add(sharedContext, nullptr);

    // Looking for a compatible baseline-sharing group.
    if (addResult.isNewEntry)
        addResult.iterator->value = std::make_unique<BaselineContext>(child, preference, ascent, descent);
    else {
        auto* context = addResult.iterator->value.get();
        context->updateSharedGroup(child, preference, ascent, descent);
    }
}

LayoutUnit GridBaselineAlignment::baselineOffsetForChild(ItemPosition preference, unsigned sharedContext, const RenderBox& child, GridAxis baselineAxis) const
{
    ASSERT(isBaselinePosition(preference));
    auto& group = baselineGroupForChild(preference, sharedContext, child, baselineAxis);
    if (group.size() > 1)
        return group.maxAscent() - logicalAscentForChild(child, baselineAxis);
    return LayoutUnit();
}

void GridBaselineAlignment::clear(GridAxis baselineAxis)
{
    if (baselineAxis == GridColumnAxis)
        m_rowAxisAlignmentContext.clear();
    else
        m_colAxisAlignmentContext.clear();
}

BaselineGroup::BaselineGroup(WritingMode blockFlow, ItemPosition childPreference)
    : m_maxAscent(0), m_maxDescent(0), m_items()
{
    m_blockFlow = blockFlow;
    m_preference = childPreference;
}

void BaselineGroup::update(const RenderBox& child, LayoutUnit ascent, LayoutUnit descent)
{
    if (m_items.add(&child).isNewEntry) {
        m_maxAscent = std::max(m_maxAscent, ascent);
        m_maxDescent = std::max(m_maxDescent, descent);
    }
}

bool BaselineGroup::isOppositeBlockFlow(WritingMode blockFlow) const
{
    switch (blockFlow) {
    case WritingMode::TopToBottomWritingMode:
        return false;
    case WritingMode::LeftToRightWritingMode:
        return m_blockFlow == WritingMode::RightToLeftWritingMode;
    case WritingMode::RightToLeftWritingMode:
        return m_blockFlow == WritingMode::LeftToRightWritingMode;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool BaselineGroup::isOrthogonalBlockFlow(WritingMode blockFlow) const
{
    switch (blockFlow) {
    case WritingMode::TopToBottomWritingMode:
        return m_blockFlow != WritingMode::TopToBottomWritingMode;
    case WritingMode::LeftToRightWritingMode:
    case WritingMode::RightToLeftWritingMode:
        return m_blockFlow == WritingMode::TopToBottomWritingMode;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool BaselineGroup::isCompatible(WritingMode childBlockFlow, ItemPosition childPreference) const
{
    ASSERT(isBaselinePosition(childPreference));
    ASSERT(size() > 0);
    return ((m_blockFlow == childBlockFlow || isOrthogonalBlockFlow(childBlockFlow)) && m_preference == childPreference) || (isOppositeBlockFlow(childBlockFlow) && m_preference != childPreference);
}

BaselineContext::BaselineContext(const RenderBox& child, ItemPosition preference, LayoutUnit ascent, LayoutUnit descent)
{
    ASSERT(isBaselinePosition(preference));
    updateSharedGroup(child, preference, ascent, descent);
}

const BaselineGroup& BaselineContext::sharedGroup(const RenderBox& child, ItemPosition preference) const
{
    ASSERT(isBaselinePosition(preference));
    return const_cast<BaselineContext*>(this)->findCompatibleSharedGroup(child, preference);
}

void BaselineContext::updateSharedGroup(const RenderBox& child, ItemPosition preference, LayoutUnit ascent, LayoutUnit descent)
{
    ASSERT(isBaselinePosition(preference));
    BaselineGroup& group = findCompatibleSharedGroup(child, preference);
    group.update(child, ascent, descent);
}

// FIXME: Properly implement baseline-group compatibility.
// See https://github.com/w3c/csswg-drafts/issues/721
BaselineGroup& BaselineContext::findCompatibleSharedGroup(const RenderBox& child, ItemPosition preference)
{
    WritingMode blockDirection = child.style().writingMode();
    for (auto& group : m_sharedGroups) {
        if (group.isCompatible(blockDirection, preference))
            return group;
    }
    m_sharedGroups.insert(0, BaselineGroup(blockDirection, preference));
    return m_sharedGroups[0];
}

} // namespace WebCore
