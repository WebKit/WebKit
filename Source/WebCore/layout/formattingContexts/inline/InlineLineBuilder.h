/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AbstractLineBuilder.h"
#include "FloatingContext.h"
#include "FormattingConstraints.h"
#include "InlineFormattingState.h"
#include "InlineLayoutState.h"
#include "InlineLine.h"
#include "InlineLineTypes.h"

namespace WebCore {
namespace Layout {

struct LineContent;
struct LineCandidate;

class LineBuilder : public AbstractLineBuilder {
public:
    LineBuilder(InlineFormattingContext&, HorizontalConstraints rootHorizontalConstraints, const InlineItems&);
    virtual ~LineBuilder() { };
    LineLayoutResult layoutInlineContent(const LineInput&, const std::optional<PreviousLine>&) final;

private:
    void candidateContentForLine(LineCandidate&, size_t inlineItemIndex, const InlineItemRange& needsLayoutRange, InlineLayoutUnit currentLogicalRight);
    InlineLayoutUnit leadingPunctuationWidthForLineCandiate(size_t firstInlineTextItemIndex, size_t candidateContentStartIndex) const;
    InlineLayoutUnit trailingPunctuationOrStopOrCommaWidthForLineCandiate(size_t lastInlineTextItemIndex, size_t layoutRangeEnd) const;

    struct UsedConstraints {
        InlineRect logicalRect;
        InlineLayoutUnit marginStart { 0 };
        OptionSet<UsedFloat> isConstrainedByFloat { };
    };
    UsedConstraints initialConstraintsForLine(const InlineRect& initialLineLogicalRect, std::optional<bool> previousLineEndsWithLineBreak) const;
    UsedConstraints floatConstrainedRect(const InlineRect& lineLogicalRect, InlineLayoutUnit marginStart) const;

    struct Result {
        InlineContentBreaker::IsEndOfLine isEndOfLine { InlineContentBreaker::IsEndOfLine::No };
        struct CommittedContentCount {
            size_t value { 0 };
            bool isRevert { false };
        };
        CommittedContentCount committedCount { };
        size_t partialTrailingContentLength { 0 };
        std::optional<InlineLayoutUnit> overflowLogicalWidth { };
    };
    enum MayOverConstrainLine : uint8_t { No, Yes, OnlyWhenFirstFloatOnLine };
    bool tryPlacingFloatBox(const Box&, MayOverConstrainLine);
    Result handleInlineContent(const InlineItemRange& needsLayoutRange, const LineCandidate&);
    Result handleRubyContent(const InlineItemRange& rubyContainerRange, InlineLayoutUnit availableWidthForCandidateContent);
    Result processLineBreakingResult(const LineCandidate&, const InlineItemRange& layoutRange, const InlineContentBreaker::Result&);
    UsedConstraints adjustedLineRectWithCandidateInlineContent(const LineCandidate&) const;
    size_t rebuildLineWithInlineContent(const InlineItemRange& needsLayoutRange, const InlineItem& lastInlineItemToAdd);
    size_t rebuildLineForTrailingSoftHyphen(const InlineItemRange& layoutRange);
    void commitPartialContent(const InlineContentBreaker::ContinuousContent::RunList&, const InlineContentBreaker::Result::PartialTrailingContent&);
    void initialize(const InlineRect& initialLineLogicalRect, const UsedConstraints&, const InlineItemRange& needsLayoutRange, const std::optional<PreviousLine>&);
    LineContent placeInlineAndFloatContent(const InlineItemRange&);
    struct InitialLetterOffsets {
        LayoutUnit capHeightOffset;
        LayoutUnit sunkenBelowFirstLineOffset;
    };
    std::optional<InitialLetterOffsets> adjustLineRectForInitialLetterIfApplicable(const Box& floatBox);
    bool isLastLineWithInlineContent(const LineContent&, size_t needsLayoutEnd, bool lineHasInlineContent) const;

    bool isFloatLayoutSuspended() const { return !m_suspendedFloats.isEmpty(); }
    bool shouldTryToPlaceFloatBox(const Box& floatBox, LayoutUnit floatBoxMarginBoxWidth, MayOverConstrainLine) const;

    bool isLineConstrainedByFloat() const { return !m_lineIsConstrainedByFloat.isEmpty(); }
    bool isFirstFormattedLine() const { return !m_previousLine.has_value(); }

    InlineFormattingContext& formattingContext() { return m_inlineFormattingContext; }
    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    const InlineLayoutState& inlineLayoutState() const;
    const BlockLayoutState& blockLayoutState() const { return inlineLayoutState().parentBlockLayoutState(); }
    FloatingState& floatingState();
    const FloatingState& floatingState() const { return const_cast<LineBuilder&>(*this).floatingState(); }
    const ElementBox& root() const;
    const RenderStyle& rootStyle() const;

private:
    std::optional<PreviousLine> m_previousLine { };
    InlineFormattingContext& m_inlineFormattingContext;
    std::optional<HorizontalConstraints> m_rootHorizontalConstraints;

    Line m_line;
    InlineRect m_lineInitialLogicalRect;
    InlineRect m_lineLogicalRect;
    InlineLayoutUnit m_lineMarginStart { 0.f };
    InlineLayoutUnit m_initialIntrusiveFloatsWidth { 0.f };
    InlineLayoutUnit m_candidateContentMaximumHeight { 0.f };
    const InlineItems& m_inlineItems;
    LineLayoutResult::PlacedFloatList m_placedFloats;
    LineLayoutResult::SuspendedFloatList m_suspendedFloats;
    std::optional<InlineTextItem> m_partialLeadingTextItem;
    std::optional<InlineLayoutUnit> m_overflowingLogicalWidth;
    Vector<const InlineItem*> m_wrapOpportunityList;
    Vector<InlineItem> m_lineSpanningInlineBoxes;
    OptionSet<UsedFloat> m_lineIsConstrainedByFloat { };
    std::optional<InlineLayoutUnit> m_initialLetterClearGap;
};

}
}
