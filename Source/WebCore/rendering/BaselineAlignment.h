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
#pragma once

#include <WebCore/LayoutUnit.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/WritingMode.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

enum class BaselineSynthesisEdge : uint8_t;
enum class FontBaseline : uint8_t;
enum class LineDirection : bool;
class RenderBox;

// Stateless CSS Box Alignment baseline helpers, used both while building baseline-sharing groups and by
// flex/grid to query a box's baseline outside any alignment context. They keep no per-context state, so
// they live apart from the BaselineAlignmentState grouping machinery.
struct BaselineAlignment {
    static FontBaseline NODELETE dominantBaseline(WritingMode);
    static WritingMode usedWritingModeForBaselineAlignment(LogicalBoxAxis alignmentContextAxis, WritingMode alignmentContainerWritingMode, WritingMode alignmentSubjectWritingMode);
    static LayoutUnit synthesizedBaseline(const RenderBox&, FontBaseline baselineType, WritingMode writingModeForSynthesis, LineDirection, BaselineSynthesisEdge);
};

// These classes are used to implement the Baseline Alignment logic, as described in the CSS Box Alignment
// specification.
// https://drafts.csswg.org/css-align/#baseline-terms
//
// A baseline-sharing group is composed of boxes that participate in baseline alignment together. This is
// possible only if they:
//
//   * Share an alignment context along an axis perpendicular to their baseline alignment axis.
//   * Have compatible baseline alignment preferences (i.e., the baselines that want to align are on the same
//     side of the alignment context).
//
// A BaselineGroup captures a baseline-sharing group's defining attributes -- its 'block flow direction' and
// 'baseline-preference' (first/last baseline) -- and answers whether a given alignment subject is compatible
// with it. The members themselves, and any value derived from them such as the max ascent, are tracked by the
// caller (the formatting context), not here.
//
class BaselineGroup {
    WTF_MAKE_TZONE_ALLOCATED(BaselineGroup);
public:
    BaselineGroup(FlowDirection, ItemPosition preference);

    // Determines whether a baseline-sharing group is compatible with an alignment subject,
    // based on its 'block-flow' and 'baseline-preference'
    bool NODELETE isCompatible(FlowDirection, ItemPosition) const;

private:
    // Determines whether the baseline-sharing group's associated block-flow is opposite (LR vs RL) to particular
    // alignment subject's writing-mode.
    bool NODELETE isOppositeBlockFlow(FlowDirection) const;

    // Determines whether the baseline-sharing group's associated block-flow is orthogonal (vertical vs horizontal)
    // to particular alignment subject's writing-mode.
    bool NODELETE isOrthogonalBlockFlow(FlowDirection) const;

    FlowDirection m_blockFlow;
    ItemPosition m_preference;
};

//
// BaselineAlignmentState groups the alignment subjects of one baseline alignment-context: given a subject's
// writing mode and baseline preference, it returns the index of the baseline-sharing group the subject belongs
// to, creating a new group when none is compatible. A formatting context creates one per baseline
// alignment-context. It holds no layout values -- ascents, offsets and membership live with the caller.
//
// https://drafts.csswg.org/css-align-3/#baseline-sharing-group
class BaselineAlignmentState {
    WTF_MAKE_TZONE_ALLOCATED(BaselineAlignmentState);
public:
    BaselineAlignmentState(LogicalBoxAxis alignmentContextAxis, WritingMode alignmentContainerWritingMode);

    // Returns the index of the baseline-sharing group the subject belongs to, creating one if none is compatible.
    size_t sharedGroupIndex(WritingMode alignmentSubjectWritingMode, ItemPosition preference) const;

private:
    size_t findCompatibleSharedGroup(WritingMode alignmentSubjectWritingMode, ItemPosition preference);

    Vector<BaselineGroup> m_sharedGroups;
    WritingMode m_alignmentContainerWritingMode;
    LogicalBoxAxis m_alignmentContextAxis;
};

} // namespace WebCore
