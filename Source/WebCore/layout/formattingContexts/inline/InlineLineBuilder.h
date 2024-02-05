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

namespace WebCore {
namespace Layout {

struct LineContent;
struct LineCandidate;

class LineBuilder : public AbstractLineBuilder {
public:
    LineBuilder(InlineFormattingContext&, HorizontalConstraints rootHorizontalConstraints, const InlineItemList&);
    virtual ~LineBuilder() { };
    LineLayoutResult layoutInlineContent(const LineInput&, const std::optional<PreviousLine>&) final;

private:
    void candidateContentForLine(LineCandidate&, size_t inlineItemIndex, const InlineItemRange& needsLayoutRange, InlineLayoutUnit currentLogicalRight);
    InlineLayoutUnit leadingPunctuationWidthForLineCandiate(size_t firstInlineTextItemIndex, size_t candidateContentStartIndex) const;
    InlineLayoutUnit trailingPunctuationOrStopOrCommaWidthForLineCandiate(size_t lastInlineTextItemIndex, size_t layoutRangeEnd) const;

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
    Result processLineBreakingResult(const LineCandidate&, const InlineItemRange& layoutRange, const InlineContentBreaker::Result&);
    struct RectAndFloatConstraints {
        InlineRect logicalRect;
        OptionSet<UsedFloat> constrainedSideSet { };
    };
    RectAndFloatConstraints floatAvoidingRect(const InlineRect& lineLogicalRect, InlineLayoutUnit lineMarginStart) const;
    RectAndFloatConstraints adjustedLineRectWithCandidateInlineContent(const LineCandidate&) const;
    size_t rebuildLineWithInlineContent(const InlineItemRange& needsLayoutRange, const InlineItem& lastInlineItemToAdd);
    size_t rebuildLineForTrailingSoftHyphen(const InlineItemRange& layoutRange);
    void commitPartialContent(const InlineContentBreaker::ContinuousContent::RunList&, const InlineContentBreaker::Result::PartialTrailingContent&);
    void initialize(const InlineRect& initialLineLogicalRect, const InlineItemRange& needsLayoutRange, const std::optional<PreviousLine>&,  std::optional<bool> previousLineEndsWithLineBreak);
    LineContent placeInlineAndFloatContent(const InlineItemRange&);
    struct InitialLetterOffsets {
        LayoutUnit capHeightOffset;
        LayoutUnit sunkenBelowFirstLineOffset;
    };
    std::optional<InitialLetterOffsets> adjustLineRectForInitialLetterIfApplicable(const Box& floatBox);
    bool isLastLineWithInlineContent(const LineContent&, size_t needsLayoutEnd, const Line::RunList&) const;

    bool isFloatLayoutSuspended() const { return !m_suspendedFloats.isEmpty(); }
    bool shouldTryToPlaceFloatBox(const Box& floatBox, LayoutUnit floatBoxMarginBoxWidth, MayOverConstrainLine) const;

    bool isLineConstrainedByFloat() const { return !m_lineIsConstrainedByFloat.isEmpty(); }
    const FloatingContext& floatingContext() const { return m_floatingContext; }

private:
    const FloatingContext& m_floatingContext;
    InlineRect m_lineInitialLogicalRect;
    InlineLayoutUnit m_lineMarginStart { 0.f };
    InlineLayoutUnit m_initialIntrusiveFloatsWidth { 0.f };
    InlineLayoutUnit m_candidateContentMaximumHeight { 0.f };
    LineLayoutResult::PlacedFloatList m_placedFloats;
    LineLayoutResult::SuspendedFloatList m_suspendedFloats;
    std::optional<InlineLayoutUnit> m_overflowingLogicalWidth;
    Vector<InlineItem, 1> m_lineSpanningInlineBoxes;
    OptionSet<UsedFloat> m_lineIsConstrainedByFloat { };
    std::optional<InlineLayoutUnit> m_initialLetterClearGap;
};

}
}
