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

class LineBuilder final : public AbstractLineBuilder {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(LineBuilder);
public:
    LineBuilder(InlineFormattingContext&, HorizontalConstraints rootHorizontalConstraints, const InlineItemList&, TextSpacingContext = { });
    virtual ~LineBuilder() { };
    LineLayoutResult layoutInlineContent(const LineInput&, const std::optional<PreviousLine>&, bool isFirstFormattedLineCandidate) final;

private:
    enum class SkipFloats : bool { No, Yes };
    void candidateContentForLine(LineCandidate&, std::pair<size_t, size_t> startEndIndex, const InlineItemRange& needsLayoutRange, InlineLayoutUnit currentLogicalRight, SkipFloats = SkipFloats::No);
    void applyShapingIfNeeded(LineCandidate&);
    Vector<std::pair<size_t, size_t>> collectShapeRanges(const LineCandidate&) const;
    void applyShapingOnRunRange(LineCandidate&, std::pair<size_t, size_t> range) const;
    void shapePartialLineCandidate(LineCandidate&, size_t trailingRunIndex) const;
    InlineLayoutUnit leadingPunctuationWidthForLineCandiate(const LineCandidate&) const;
    InlineLayoutUnit trailingPunctuationOrStopOrCommaWidthForLineCandiate(const LineCandidate&, size_t startIndexAfterCandidateContent,  size_t layoutRangeEnd) const;

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
    Result handleInlineContent(const InlineItemRange& needsLayoutRange, LineCandidate&);
    void handleBlockContent(const InlineItem& blockItem);
    Result processLineBreakingResult(LineCandidate&, const InlineItemRange& layoutRange, const InlineContentBreaker::Result&);
    struct RectAndFloatConstraints {
        InlineRect logicalRect;
        OptionSet<UsedFloat> constrainedSideSet { };
    };
    RectAndFloatConstraints floatAvoidingRect(const InlineRect& lineLogicalRect, InlineLayoutUnit lineMarginStart) const;
    RectAndFloatConstraints adjustedLineRectWithCandidateInlineContent(const LineCandidate&) const;

    Result tryPlacingCandidateInlineContentOnLine(const InlineItemRange& needsLayoutRange, LineCandidate&);
    void commitCandidateContent(LineCandidate&, std::optional<InlineContentBreaker::Result::PartialTrailingContent>);
    size_t rebuildLineWithInlineContent(const InlineItemRange& needsLayoutRange, const InlineItem& lastInlineItemToAdd);
    size_t rebuildLineForTrailingSoftHyphen(const InlineItemRange& layoutRange);
    void initialize(const InlineRect& initialLineLogicalRect, const InlineItemRange& needsLayoutRange, const std::optional<PreviousLine>&, bool isFirstFormattedLineCandidate);
    UniqueRef<LineContent> placeInlineAndFloatContent(const InlineItemRange&);
    struct InitialLetterOffsets {
        LayoutUnit capHeightOffset;
        LayoutUnit sunkenBelowFirstLineOffset;
    };
    std::optional<InitialLetterOffsets> adjustLineRectForInitialLetterIfApplicable(const Box& floatBox);
    bool isLastLineWithInlineContent(const LineContent&, size_t needsLayoutEnd, const Line::RunList&) const;
    InlineContentBreaker::Result handleInlineContentWithClonedDecoration(const LineCandidate&, InlineContentBreaker::LineStatus);
    InlineLayoutUnit clonedDecorationAtBreakingPosition(const InlineContentBreaker::ContinuousContent::RunList&, const InlineContentBreaker::Result::PartialTrailingContent&) const;
    InlineLayoutUnit placedClonedDecorationWidth(const InlineContentBreaker::ContinuousContent::RunList&) const;
    enum class ShouldResetMarginValues : bool { No, Yes };
    bool applyMarginInBlockDirectionIfNeeded(ShouldResetMarginValues);

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
    TextSpacingContext m_textSpacingContext { };
    bool m_hasAdjustedLineRectWithBlockMargin { false };
};

}
}
