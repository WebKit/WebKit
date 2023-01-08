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

#include "BlockLayoutState.h"
#include "FloatingContext.h"
#include "FormattingConstraints.h"
#include "InlineContentBreaker.h"
#include "InlineFormattingState.h"
#include "InlineLine.h"

namespace WebCore {
namespace Layout {

struct LineCandidate;

class LineBuilder {
public:
    LineBuilder(InlineFormattingContext&, BlockLayoutState&, HorizontalConstraints rootHorizontalConstraints, const InlineItems&, std::optional<IntrinsicWidthMode> = std::nullopt);
    LineBuilder(const InlineFormattingContext&, const InlineItems&, std::optional<IntrinsicWidthMode>);

    struct InlineItemRange {
        bool isEmpty() const { return start == end; }
        size_t size() const { return end - start; }
        size_t start { 0 };
        size_t end { 0 };
    };
    struct LineInput {
        InlineItemRange needsLayoutRange;
        InlineRect initialLogicalRect;

        enum class LineEndingEllipsisPolicy : uint8_t {
            No,
            WhenContentOverflowsInInlineDirection,
            WhenContentOverflowsInBlockDirection,
            // FIXME: This should be used when we realize the last line of this IFC is where the content is truncated (sibling IFC has more lines).
            Always
        };
        LineEndingEllipsisPolicy ellipsisPolicy { LineEndingEllipsisPolicy::No };
    };
    struct PartialContent {
        PartialContent(size_t, std::optional<InlineLayoutUnit>);

        size_t length { 0 };
        std::optional<InlineLayoutUnit> width { };
    };
    using FloatList = Vector<const InlineItem*>;
    struct PreviousLine {
        bool endsWithLineBreak { false };
        TextDirection inlineBaseDirection { TextDirection::LTR };
        std::optional<PartialContent> partialOverflowingContent { };
        FloatList overflowingFloats;
        // Content width measured during line breaking (avoid double-measuring).
        std::optional<InlineLayoutUnit> trailingOverflowingContentWidth { };
    };
    struct LineContent {
        InlineItemRange inlineItemRange;
        std::optional<PartialContent> partialOverflowingContent { };
        std::optional<InlineLayoutUnit> trailingOverflowingContentWidth;
        FloatList placedFloats;
        FloatList overflowingFloats;
        bool hasIntrusiveFloat { false };
        InlineLayoutUnit lineInitialLogicalLeftIncludingIntrusiveFloats { 0.f };
        InlineLayoutPoint lineLogicalTopLeft;
        InlineLayoutUnit lineLogicalWidth { 0.f };
        InlineLayoutUnit contentLogicalLeft { 0.f };
        InlineLayoutUnit contentLogicalWidth { 0.f };
        InlineLayoutUnit contentLogicalRightIncludingNegativeMargin { 0.f }; // Note that with negative horizontal margin value, contentLogicalLeft + contentLogicalWidth is not necessarily contentLogicalRight.
        struct HangingContent {
            bool shouldContributeToScrollableOverflow { false };
            InlineLayoutUnit width { 0.f };
        };
        HangingContent hangingContent;
        enum class FirstFormattedLine : uint8_t {
            No,
            WithinIFC,
            WithinBFC
        };
        FirstFormattedLine isFirstFormattedLine { FirstFormattedLine::WithinIFC };
        bool isLastLineWithInlineContent { true };
        size_t nonSpanningInlineLevelBoxCount { 0 };
        Vector<int32_t> visualOrderList;
        TextDirection inlineBaseDirection { TextDirection::LTR };
        bool lineNeedsTrailingEllipsis { false };
        const Line::RunList& runs;
    };
    LineContent layoutInlineContent(const LineInput&, const std::optional<PreviousLine>&);

    struct IntrinsicContent {
        InlineItemRange inlineItemRange;
        InlineLayoutUnit logicalWidth { 0 };
        std::optional<PartialContent> partialOverflowingContent { };
        FloatList placedFloats;
    };
    IntrinsicContent computedIntrinsicWidth(const InlineItemRange&, const std::optional<PreviousLine>&);

private:
    void candidateContentForLine(LineCandidate&, size_t inlineItemIndex, const InlineItemRange& needsLayoutRange, InlineLayoutUnit currentLogicalRight);
    InlineLayoutUnit leadingPunctuationWidthForLineCandiate(size_t firstInlineTextItemIndex, size_t candidateContentStartIndex) const;
    InlineLayoutUnit trailingPunctuationOrStopOrCommaWidthForLineCandiate(size_t lastInlineTextItemIndex, size_t layoutRangeEnd) const;
    size_t nextWrapOpportunity(size_t startIndex, const LineBuilder::InlineItemRange& layoutRange) const;

    struct UsedConstraints {
        InlineRect logicalRect;
        InlineLayoutUnit marginStart { 0 };
        bool isConstrainedByFloat { false };
    };
    UsedConstraints initialConstraintsForLine(const InlineRect& initialLineLogicalRect, std::optional<bool> previousLineEndsWithLineBreak) const;
    FloatingContext::Constraints floatConstraints(const InlineRect& lineLogicalRect) const;

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
    LayoutUnit adjustGeometryForInitialLetterIfNeeded(const Box& floatBox);
    enum LineBoxConstraintApplies : uint8_t { Yes, No };
    bool tryPlacingFloatBox(const InlineItem&, LineBoxConstraintApplies);
    Result handleInlineContent(InlineContentBreaker&, const InlineItemRange& needsLayoutRange, const LineCandidate&);
    std::tuple<InlineRect, bool> lineBoxForCandidateInlineContent(const LineCandidate&) const;
    size_t rebuildLineWithInlineContent(const InlineItemRange& needsLayoutRange, const InlineItem& lastInlineItemToAdd);
    size_t rebuildLineForTrailingSoftHyphen(const InlineItemRange& layoutRange);
    void commitPartialContent(const InlineContentBreaker::ContinuousContent::RunList&, const InlineContentBreaker::Result::PartialTrailingContent&);
    void initialize(const InlineRect& initialLineLogicalRect, const UsedConstraints&, const InlineItemRange& needsLayoutRange, const std::optional<PreviousLine>&);
    struct CommittedContent {
        size_t itemCount { 0 };
        size_t partialTrailingContentLength { 0 };
        std::optional<InlineLayoutUnit> overflowLogicalWidth { };
    };
    CommittedContent placeInlineContent(const InlineItemRange&);
    InlineItemRange close(const InlineItemRange& needsLayoutRange, LineInput::LineEndingEllipsisPolicy, const CommittedContent&);

    InlineLayoutUnit inlineItemWidth(const InlineItem&, InlineLayoutUnit contentLogicalLeft) const;
    bool isLastLineWithInlineContent(const InlineItemRange& lineRange, size_t lastInlineItemIndex, bool hasPartialTrailingContent) const;

    std::optional<IntrinsicWidthMode> intrinsicWidthMode() const { return m_intrinsicWidthMode; }
    bool isInIntrinsicWidthMode() const { return !!intrinsicWidthMode(); }

    TextDirection inlineBaseDirectionForLineContent() const;
    InlineLayoutUnit horizontalAlignmentOffset(bool isLastLine) const;

    bool isFirstFormattedLine() const { return !m_previousLine.has_value(); }

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    InlineFormattingState* formattingState() { return m_inlineFormattingState; }
    const BlockLayoutState* blockLayoutState() const { return m_blockLayoutState; }
    FloatingState* floatingState() { return m_blockLayoutState ? &m_blockLayoutState->floatingState() : nullptr; }
    const FloatingState* floatingState() const { return const_cast<LineBuilder&>(*this).floatingState(); }
    const ElementBox& root() const;
    const LayoutState& layoutState() const;
    const RenderStyle& rootStyle() const;

private:
    std::optional<PreviousLine> m_previousLine { };
    std::optional<IntrinsicWidthMode> m_intrinsicWidthMode;
    const InlineFormattingContext& m_inlineFormattingContext;
    InlineFormattingState* m_inlineFormattingState { nullptr };
    BlockLayoutState* m_blockLayoutState { nullptr };
    std::optional<HorizontalConstraints> m_rootHorizontalConstraints;

    Line m_line;
    InlineRect m_lineLogicalRect;
    InlineLayoutUnit m_lineMarginStart { 0.f };
    InlineLayoutUnit m_lineInitialLogicalLeft { 0.f };
    InlineLayoutUnit m_initialIntrusiveFloatsWidth { 0.f };
    const InlineItems& m_inlineItems;
    FloatList m_placedFloats;
    FloatList m_overflowingFloats;
    std::optional<InlineTextItem> m_partialLeadingTextItem;
    std::optional<InlineLayoutUnit> m_overflowingLogicalWidth;
    Vector<const InlineItem*> m_wrapOpportunityList;
    Vector<InlineItem> m_lineSpanningInlineBoxes;
    unsigned m_successiveHyphenatedLineCount { 0 };
    bool m_lineIsConstrainedByFloat { false };
};

inline LineBuilder::PartialContent::PartialContent(size_t length, std::optional<InlineLayoutUnit> width)
    : length(length)
    , width(width)
{
}

}
}
