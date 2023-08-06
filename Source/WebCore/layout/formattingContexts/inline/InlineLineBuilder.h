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

#include "FloatingContext.h"
#include "FormattingConstraints.h"
#include "InlineContentBreaker.h"
#include "InlineFormattingState.h"
#include "InlineLayoutState.h"
#include "InlineLine.h"
#include "InlineLineTypes.h"

namespace WebCore {
namespace Layout {

struct LineContent;
struct LineCandidate;

class LineBuilder {
public:
    LineBuilder(const InlineFormattingContext&, const InlineLayoutState&, FloatingState&, HorizontalConstraints rootHorizontalConstraints, const InlineItems&, std::optional<IntrinsicWidthMode> = std::nullopt);

    struct LineInput {
        InlineItemRange needsLayoutRange;
        InlineRect initialLogicalRect;
    };
    using PlacedFloatList = FloatingState::FloatList;
    using SuspendedFloatList = Vector<const Box*>;
    struct LayoutResult {
        InlineItemRange inlineItemRange;
        const Line::RunList& inlineContent;

        struct FloatContent {
            PlacedFloatList placedFloats;
            SuspendedFloatList suspendedFloats;
            bool hasIntrusiveFloat { false };
        };
        FloatContent floatContent { };

        struct ContentGeometry {
            InlineLayoutUnit logicalLeft { 0.f };
            InlineLayoutUnit logicalWidth { 0.f };
            InlineLayoutUnit logicalRightIncludingNegativeMargin { 0.f }; // Note that with negative horizontal margin value, contentLogicalLeft + contentLogicalWidth is not necessarily contentLogicalRight.
            std::optional<InlineLayoutUnit> trailingOverflowingContentWidth { };
        };
        ContentGeometry contentGeometry { };

        struct LineGeometry {
            InlineLayoutPoint logicalTopLeft;
            InlineLayoutUnit logicalWidth { 0.f };
            InlineLayoutUnit initialLogicalLeftIncludingIntrusiveFloats { 0.f };
            std::optional<InlineLayoutUnit> initialLetterClearGap;
        };
        LineGeometry lineGeometry { };

        struct HangingContent {
            bool shouldContributeToScrollableOverflow { false };
            InlineLayoutUnit logicalWidth { 0.f };
        };
        HangingContent hangingContent { };

        struct Directionality {
            Vector<int32_t> visualOrderList;
            TextDirection inlineBaseDirection { TextDirection::LTR };
        };
        Directionality directionality { };

        struct IsFirstLast {
            enum class FirstFormattedLine : uint8_t {
                No,
                WithinIFC,
                WithinBFC
            };
            FirstFormattedLine isFirstFormattedLine { FirstFormattedLine::WithinIFC };
            bool isLastLineWithInlineContent { true };
        };
        IsFirstLast isFirstLast { };
        // Misc
        size_t nonSpanningInlineLevelBoxCount { 0 };
        std::optional<InlineLayoutUnit> hintForNextLineTopToAvoidIntrusiveFloat { }; // This is only used for cases when intrusive floats prevent any content placement at current vertical position.
    };
    LayoutResult layoutInlineContent(const LineInput&, const std::optional<PreviousLine>&);

private:
    void candidateContentForLine(LineCandidate&, size_t inlineItemIndex, const InlineItemRange& needsLayoutRange, InlineLayoutUnit currentLogicalRight);
    InlineLayoutUnit leadingPunctuationWidthForLineCandiate(size_t firstInlineTextItemIndex, size_t candidateContentStartIndex) const;
    InlineLayoutUnit trailingPunctuationOrStopOrCommaWidthForLineCandiate(size_t lastInlineTextItemIndex, size_t layoutRangeEnd) const;
    size_t nextWrapOpportunity(size_t startIndex, const InlineItemRange& layoutRange) const;

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
    enum MayOverConstrainLine : bool { No, Yes };
    bool tryPlacingFloatBox(const Box&, MayOverConstrainLine);
    Result handleInlineContent(InlineContentBreaker&, const InlineItemRange& needsLayoutRange, const LineCandidate&);
    std::tuple<InlineRect, bool> adjustedLineRectWithCandidateInlineContent(const LineCandidate&) const;
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

    InlineLayoutUnit inlineItemWidth(const InlineItem&, InlineLayoutUnit contentLogicalLeft) const;
    bool isLastLineWithInlineContent(const InlineItemRange& lineRange, size_t lastInlineItemIndex, bool hasPartialTrailingContent) const;

    std::optional<IntrinsicWidthMode> intrinsicWidthMode() const { return m_intrinsicWidthMode; }
    bool isInIntrinsicWidthMode() const { return !!intrinsicWidthMode(); }

    TextDirection inlineBaseDirectionForLineContent() const;
    InlineLayoutUnit horizontalAlignmentOffset(bool isLastLine) const;

    bool isFloatLayoutSuspended() const { return !m_suspendedFloats.isEmpty(); }
    bool shouldTryToPlaceFloatBox(const Box& floatBox, LayoutUnit floatBoxMarginBoxWidth, MayOverConstrainLine) const;

    bool isFirstFormattedLine() const { return !m_previousLine.has_value(); }

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    const BlockLayoutState& blockLayoutState() const { return m_inlineLayoutState.parentBlockLayoutState(); }
    FloatingState& floatingState() { return m_floatingState; }
    const FloatingState& floatingState() const { return const_cast<LineBuilder&>(*this).floatingState(); }
    const ElementBox& root() const;
    const RenderStyle& rootStyle() const;

private:
    std::optional<PreviousLine> m_previousLine { };
    std::optional<IntrinsicWidthMode> m_intrinsicWidthMode;
    const InlineFormattingContext& m_inlineFormattingContext;
    const InlineLayoutState& m_inlineLayoutState;
    FloatingState& m_floatingState;
    std::optional<HorizontalConstraints> m_rootHorizontalConstraints;

    Line m_line;
    InlineRect m_lineInitialLogicalRect;
    InlineRect m_lineLogicalRect;
    InlineLayoutUnit m_lineMarginStart { 0.f };
    InlineLayoutUnit m_initialIntrusiveFloatsWidth { 0.f };
    InlineLayoutUnit m_candidateInlineContentEnclosingHeight { 0.f };
    const InlineItems& m_inlineItems;
    PlacedFloatList m_placedFloats;
    SuspendedFloatList m_suspendedFloats;
    std::optional<InlineTextItem> m_partialLeadingTextItem;
    std::optional<InlineLayoutUnit> m_overflowingLogicalWidth;
    Vector<const InlineItem*> m_wrapOpportunityList;
    Vector<InlineItem> m_lineSpanningInlineBoxes;
    unsigned m_successiveHyphenatedLineCount { 0 };
    bool m_lineIsConstrainedByFloat { false };
    std::optional<InlineLayoutUnit> m_initialLetterClearGap;
};

}
}
