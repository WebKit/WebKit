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

#include "LayoutUnit.h"
#include "RenderStyleConstants.h"
#include "WritingMode.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

enum class BaselineSynthesisEdge : uint8_t;
enum class FontBaseline : uint8_t;
enum class LineDirection : bool;
class RenderBox;

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
// Once the BaselineGroup is instantiated, defined by a 'block flow direction' and a 'baseline-preference'
// (first/last baseline), it's ready to collect the alignment subjects that will participate in the Baseline Alignment logic.
//
class BaselineGroup {
    WTF_MAKE_TZONE_ALLOCATED(BaselineGroup);
public:
    // It stores an alignment subject(if not already present) and update the max_ascent
    // associated to this baseline-sharing group.
    void update(const RenderBox&, LayoutUnit ascent);
    LayoutUnit maxAscent() const { return m_maxAscent; }
    int computeSize() const { return m_alignmentSubjects.computeSize(); }
    auto begin() LIFETIME_BOUND { return m_alignmentSubjects.begin(); }
    auto end() LIFETIME_BOUND { return m_alignmentSubjects.end(); }

private:
    friend class BaselineAlignmentState;
    BaselineGroup(FlowDirection, ItemPosition childPreference);

    // Determines whether a baseline-sharing group is compatible with an alignment subject,
    // based on its 'block-flow' and 'baseline-preference'
    bool isCompatible(FlowDirection, ItemPosition) const;

    // Determines whether the baseline-sharing group's associated block-flow is opposite (LR vs RL) to particular
    // alignment subject's writing-mode.
    bool isOppositeBlockFlow(FlowDirection) const;

    // Determines whether the baseline-sharing group's associated block-flow is orthogonal (vertical vs horizontal)
    // to particular alignment subject's writing-mode.
    bool isOrthogonalBlockFlow(FlowDirection) const;

    FlowDirection m_blockFlow;
    ItemPosition m_preference;
    LayoutUnit m_maxAscent;
    SingleThreadWeakHashSet<RenderBox> m_alignmentSubjects;
};

//
// BaselineAlignmentState provides an API to interact with baseline sharing groups in various
// ways such as adding alignment subjects to appropriate ones and querying the baseline
// sharing group for an alignment subject. A BaselineAlignmentState should be created by a formatting
// context to use for each of its baseline alignment contexts.
//
// https://drafts.csswg.org/css-align-3/#baseline-sharing-group
// A Baseline alignment-context may handle several baseline-sharing groups. In order to create an instance, we
// need to pass the required data to define the first baseline-sharing group; a BaselineAlignmentState must have at
// least one baseline-sharing group.
//
// By adding new alignment subjects to a BaselineAlignmentState, the baseline-sharing
// groups it handles are automatically updated, if there is one that is compatible with
// such alignment subject. Otherwise, a new baseline-sharing group is created, compatible with the new
// alignment subject.
class BaselineAlignmentState {
    WTF_MAKE_TZONE_ALLOCATED(BaselineAlignmentState);
public:
    BaselineAlignmentState(const RenderBox& alignmentSubject, ItemPosition preference, LayoutUnit ascent, LogicalBoxAxis alignmentContextAxis, WritingMode alignmentContainerWritingMode);
    const BaselineGroup& sharedGroup(const RenderBox& alignmentSubject, ItemPosition preference) const;

    void updateSharedGroup(const RenderBox& alignmentSubject, ItemPosition preference, LayoutUnit ascent);
    Vector<BaselineGroup>& sharedGroups();

    static FontBaseline dominantBaseline(WritingMode);
    static WritingMode usedWritingModeForBaselineAlignment(LogicalBoxAxis alignmentContextAxis, WritingMode alignmentContainerWritingMode, WritingMode alignmentSubjectWritingMode);
    static LayoutUnit synthesizedBaseline(const RenderBox&, FontBaseline baselineType, WritingMode writingModeForSynthesis, LineDirection, BaselineSynthesisEdge);

private:
    BaselineGroup& findCompatibleSharedGroup(const RenderBox& alignmentSubject, ItemPosition preference);

    Vector<BaselineGroup> m_sharedGroups;
    WritingMode m_alignmentContainerWritingMode;
    LogicalBoxAxis m_alignmentContextAxis;
};

} // namespace WebCore
