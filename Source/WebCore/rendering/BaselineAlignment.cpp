/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "BaselineAlignment.h"

#include "BaselineAlignmentInlines.h"
#include "RenderBox.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderStyleInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BaselineGroup);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BaselineAlignmentState);

BaselineGroup::BaselineGroup(FlowDirection blockFlow, ItemPosition alignmentSubjectPreference)
    : m_maxAscent(0), m_alignmentSubjects()
{
    m_blockFlow = blockFlow;
    m_preference = alignmentSubjectPreference;
}

void BaselineGroup::update(const RenderBox& alignmentSubject, LayoutUnit ascent)
{
    if (m_alignmentSubjects.add(alignmentSubject).isNewEntry)
        m_maxAscent = std::max(m_maxAscent, ascent);
}

bool BaselineGroup::isOppositeBlockFlow(FlowDirection blockFlow) const
{
    switch (blockFlow) {
    case FlowDirection::TopToBottom:
        return false;
    case FlowDirection::LeftToRight:
        return m_blockFlow == FlowDirection::RightToLeft;
    case FlowDirection::RightToLeft:
        return m_blockFlow == FlowDirection::LeftToRight;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool BaselineGroup::isOrthogonalBlockFlow(FlowDirection blockFlow) const
{
    switch (blockFlow) {
    case FlowDirection::TopToBottom:
        return m_blockFlow != FlowDirection::TopToBottom;
    case FlowDirection::LeftToRight:
    case FlowDirection::RightToLeft:
        return m_blockFlow == FlowDirection::TopToBottom;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool BaselineGroup::isCompatible(FlowDirection alignmentSubjectBlockFlow, ItemPosition alignmentSubjectPreference) const
{
    ASSERT(isBaselinePosition(alignmentSubjectPreference));
    ASSERT(computeSize() > 0);
    return ((m_blockFlow == alignmentSubjectBlockFlow || isOrthogonalBlockFlow(alignmentSubjectBlockFlow)) && m_preference == alignmentSubjectPreference)
        || (isOppositeBlockFlow(alignmentSubjectBlockFlow) && m_preference != alignmentSubjectPreference);
}

BaselineAlignmentState::BaselineAlignmentState(const RenderBox& alignmentSubject, ItemPosition preference, LayoutUnit ascent, LogicalBoxAxis alignmentContextAxis, WritingMode alignmentContainerWritingMode)
    : m_alignmentContainerWritingMode(alignmentContainerWritingMode)
    , m_alignmentContextAxis(alignmentContextAxis)
{
    ASSERT(isBaselinePosition(preference));
    updateSharedGroup(alignmentSubject, preference, ascent);
}

const BaselineGroup& BaselineAlignmentState::sharedGroup(const RenderBox& alignmentSubject, ItemPosition preference) const
{
    ASSERT(isBaselinePosition(preference));
    return const_cast<BaselineAlignmentState*>(this)->findCompatibleSharedGroup(alignmentSubject, preference);
}

Vector<BaselineGroup>& BaselineAlignmentState::sharedGroups()
{
    return m_sharedGroups;
}

void BaselineAlignmentState::updateSharedGroup(const RenderBox& alignmentSubject, ItemPosition preference, LayoutUnit ascent)
{
    ASSERT(isBaselinePosition(preference));
    BaselineGroup& group = findCompatibleSharedGroup(alignmentSubject, preference);
    group.update(alignmentSubject, ascent);
}

FontBaseline BaselineAlignmentState::dominantBaseline(WritingMode writingMode)
{
    // https://drafts.csswg.org/css-inline-3/#alignment-baseline-property
    // https://drafts.csswg.org/css-inline-3/#dominant-baseline-property
    return writingMode.prefersCentralBaseline() ? FontBaseline::Central : FontBaseline::Alphabetic;
}

LayoutUnit BaselineAlignmentState::synthesizedBaseline(const RenderBox& box, FontBaseline baselineType, WritingMode writingModeForSynthesis, LineDirection lineDirection, BaselineSynthesisEdge edge)
{
    auto boxSize = lineDirection == LineDirection::Horizontal ? box.height() : box.width();
    if (edge == BaselineSynthesisEdge::ContentBox)
        boxSize -= lineDirection == LineDirection::Horizontal ? box.verticalBorderAndPaddingExtent() : box.horizontalBorderAndPaddingExtent();
    else if (edge == BaselineSynthesisEdge::MarginBox)
        boxSize += lineDirection == LineDirection::Horizontal ? box.verticalMarginExtent() : box.horizontalMarginExtent();

    if (baselineType == FontBaseline::Alphabetic) {
        // When synthesizing the alphabetic baseline for a box we are determining the distance
        // to the line-under edge. For a box with vertical-lr writing mode the location
        // of the line-under edge should be the same as the box's block-start edge. For
        // vertical-rl writing mode we need the box's size since they are on opposiate sides.
        auto shouldTreatAsHorizontal = lineDirection == LineDirection::Horizontal || writingModeForSynthesis.computedWritingMode() == StyleWritingMode::VerticalRl;
        return shouldTreatAsHorizontal ? boxSize : LayoutUnit();
    }
    return boxSize / 2;
}

WritingMode BaselineAlignmentState::usedWritingModeForBaselineAlignment(LogicalBoxAxis alignmentContextAxis,
    WritingMode alignmentContainerWritingMode, WritingMode aligmentSubjectWritingMode)
{

    auto isAlignmentSubjectBlockFlowParallelToAlignmentContextAxis = [&] {
        if (alignmentContextAxis == LogicalBoxAxis::Block)
            return !alignmentContainerWritingMode.isOrthogonal(aligmentSubjectWritingMode);
        return alignmentContainerWritingMode.isOrthogonal(aligmentSubjectWritingMode);
    };

    // css-align-3: 9.1. Determining the Baselines of a Box
    // In general, the writing mode of the box, shape, or other object being aligned is used to determine
    // the line-under and line-over edges for synthesis...
    if (!isAlignmentSubjectBlockFlowParallelToAlignmentContextAxis())
        return aligmentSubjectWritingMode;

    // ... However, when that writing mode’s block flow direction
    // is parallel to the axis of the alignment context, an axis-compatible writing mode must be assumed:
    //

    // If the box establishing the alignment context has a block flow direction that is orthogonal to the
    // axis of the alignment context, use its writing mode.
    if (alignmentContextAxis == LogicalBoxAxis::Inline)
        return alignmentContainerWritingMode;

    // Otherwise:
    // If the box’s own writing mode is vertical, assume horizontal-tb.
    // If the box’s own writing mode is horizontal, assume vertical-lr if
    // direction is ltr and vertical-rl if direction is rtl.
    if (!aligmentSubjectWritingMode.isHorizontal())
        return { };
    auto styleWritingMode = alignmentContainerWritingMode.isBidiLTR() ? StyleWritingMode::VerticalLr : StyleWritingMode::VerticalRl;
    return { styleWritingMode, TextDirection::LTR, TextOrientation::Mixed };
}

BaselineGroup& BaselineAlignmentState::findCompatibleSharedGroup(const RenderBox& alignmentSubject, ItemPosition preference)
{
    auto usedWritingModeForBaselineAlignment = this->usedWritingModeForBaselineAlignment(m_alignmentContextAxis, m_alignmentContainerWritingMode, alignmentSubject.writingMode());
    auto blockFlowDirection = usedWritingModeForBaselineAlignment.blockDirection();
    for (auto& group : m_sharedGroups) {
        if (group.isCompatible(blockFlowDirection, preference))
            return group;
    }
    m_sharedGroups.insert(0, BaselineGroup(blockFlowDirection, preference));
    return m_sharedGroups[0];
}

} // namespace WebCore
