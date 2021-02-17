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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineContentBreaker.h"
#include "InlineFormattingState.h"
#include "InlineLine.h"

namespace WebCore {
namespace Layout {

class FloatingContext;
struct LineCandidate;

class LineBuilder {
public:
    LineBuilder(InlineFormattingContext&, FloatingState&, HorizontalConstraints rootHorizontalConstraints, const InlineItems&);
    LineBuilder(const InlineFormattingContext&, const InlineItems&);

    struct InlineItemRange {
        bool isEmpty() const { return start == end; }
        size_t size() const { return end - start; }
        size_t start { 0 };
        size_t end { 0 };
    };
    using FloatList = Vector<const Box*>;
    struct LineContent {
        InlineItemRange inlineItemRange;
        size_t partialTrailingContentLength { 0 };
        Optional<InlineLayoutUnit> overflowLogicalWidth;
        const FloatList& floats;
        bool hasIntrusiveFloat { false };
        InlineLayoutPoint logicalTopLeft;
        InlineLayoutUnit lineLogicalWidth;
        InlineLayoutUnit contentLogicalWidth;
        bool isLineConsideredEmpty { true };
        bool isLastLineWithInlineContent { true };
        const Line::RunList& runs;
    };
    LineContent layoutInlineContent(const InlineItemRange&, size_t partialLeadingContentLength, Optional<InlineLayoutUnit> leadingLogicalWidth, const InlineRect& initialLineLogicalRect, bool isFirstLine);

    struct IntrinsicContent {
        InlineItemRange inlineItemRange;
        InlineLayoutUnit logicalWidth { 0 };
        const FloatList& floats;
    };
    IntrinsicContent computedIntrinsicWidth(const InlineItemRange&, InlineLayoutUnit availableWidth);

private:
    void candidateContentForLine(LineCandidate&, size_t inlineItemIndex, const InlineItemRange& needsLayoutRange, size_t overflowLength, Optional<InlineLayoutUnit> leadingLogicalWidth, InlineLayoutUnit currentLogicalRight);
    size_t nextWrapOpportunity(size_t startIndex, const LineBuilder::InlineItemRange& layoutRange) const;

    struct Result {
        InlineContentBreaker::IsEndOfLine isEndOfLine { InlineContentBreaker::IsEndOfLine::No };
        struct CommittedContentCount {
            size_t value { 0 };
            bool isRevert { false };
        };
        CommittedContentCount committedCount { };
        size_t partialTrailingContentLength { 0 };
        Optional<InlineLayoutUnit> overflowLogicalWidth { };
    };
    struct UsedConstraints {
        InlineRect logicalRect;
        bool isConstrainedByFloat { false };
    };
    UsedConstraints initialConstraintsForLine(const InlineRect& initialLineLogicalRect, bool isFirstLine) const;
    Optional<HorizontalConstraints> floatConstraints(const InlineRect& lineLogicalRect) const;

    void handleFloatContent(const InlineItem&);
    Result handleInlineContent(InlineContentBreaker&, const InlineItemRange& needsLayoutRange, const LineCandidate&);
    size_t rebuildLine(const InlineItemRange& needsLayoutRange, const InlineItem& lastInlineItemToAdd);
    size_t rebuildLineForTrailingSoftHyphen(const InlineItemRange& layoutRange);
    void commitPartialContent(const InlineContentBreaker::ContinuousContent::RunList&, const InlineContentBreaker::Result::PartialTrailingContent&);
    void initialize(const UsedConstraints&);
    struct CommittedContent {
        size_t inlineItemCount { 0 };
        size_t partialTrailingContentLength { 0 };
        Optional<InlineLayoutUnit> overflowLogicalWidth { };
    };
    CommittedContent placeInlineContent(const InlineItemRange&, size_t partialLeadingContentLength, Optional<InlineLayoutUnit> overflowLogicalWidth);
    InlineItemRange close(const InlineItemRange& needsLayoutRange, const CommittedContent&);

    InlineLayoutUnit inlineItemWidth(const InlineItem&, InlineLayoutUnit contentLogicalLeft) const;
    bool isLastLineWithInlineContent(const InlineItemRange& lineRange, size_t lastInlineItemIndex, bool hasPartialTrailingContent) const;

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    InlineFormattingState* formattingState() { return m_inlineFormattingState; }
    FloatingState* floatingState() { return m_floatingState; }
    const FloatingState* floatingState() const { return m_floatingState; }
    const ContainerBox& root() const;
    const LayoutState& layoutState() const;

    const InlineFormattingContext& m_inlineFormattingContext;
    InlineFormattingState* m_inlineFormattingState { nullptr };
    FloatingState* m_floatingState { nullptr };
    Optional<HorizontalConstraints> m_rootHorizontalConstraints;

    Line m_line;
    InlineRect m_lineLogicalRect;
    const InlineItems& m_inlineItems;
    FloatList m_floats;
    Optional<InlineTextItem> m_partialLeadingTextItem;
    Vector<const InlineItem*> m_wrapOpportunityList;
    unsigned m_successiveHyphenatedLineCount { 0 };
    bool m_contentIsConstrainedByFloat { false };
};

}
}
#endif
