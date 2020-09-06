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

#include "InlineLine.h"
#include "InlineLineBox.h"
#include "InlineLineBreaker.h"

namespace WebCore {
namespace Layout {

class FloatingContext;
struct LineCandidate;

class LineBuilder {
public:
    LineBuilder(const InlineFormattingContext&, const FloatingContext&, const ContainerBox& formattingContextRoot, const InlineItems&);

    struct InlineItemRange {
        bool isEmpty() const { return start == end; }
        size_t size() const { return end - start; }
        size_t start { 0 };
        size_t end { 0 };
    };
    struct LineContent {
        struct PartialContent {
            bool trailingContentHasHyphen { false };
            unsigned overflowContentLength { 0 };
        };
        Optional<PartialContent> partialContent;
        InlineItemRange inlineItemRange;
        struct Float {
            bool isIntrusive { true };
            const InlineItem* item { nullptr };
        };
        using FloatList = Vector<Float>;
        const FloatList& floats;
        bool hasIntrusiveFloat { false };
        const LineBox lineBox;
        const Line::RunList& runs;
    };
    LineContent layoutInlineContent(const InlineItemRange&, Optional<unsigned> partialLeadingContentLength, const FormattingContext::ConstraintsForInFlowContent& initialLineConstraints, bool isFirstLine);

    struct IntrinsicContent {
        InlineItemRange inlineItemRange;
        InlineLayoutUnit logicalWidth { 0 };
    };
    IntrinsicContent computedIntrinsicWidth(const InlineItemRange&, InlineLayoutUnit availableWidth);

private:
    void nextContentForLine(LineCandidate&, unsigned inlineItemIndex, const InlineItemRange& needsLayoutRange, Optional<unsigned> overflowLength, InlineLayoutUnit availableLineWidth, InlineLayoutUnit currentLogicalRight);
    struct Result {
        LineBreaker::IsEndOfLine isEndOfLine { LineBreaker::IsEndOfLine::No };
        struct CommittedContentCount {
            size_t value { 0 };
            bool isRevert { false };
        };
        CommittedContentCount committedCount { };
        Optional <LineContent::PartialContent> partialContent { };
    };
    enum class CommitIntrusiveFloatsOnly { No, Yes };
    struct UsedConstraints {
        InlineLayoutUnit logicalLeft { 0 };
        InlineLayoutUnit availableLogicalWidth { 0 };
        bool isConstrainedByFloat { false };
    };
    UsedConstraints constraintsForLine(const FormattingContext::ConstraintsForInFlowContent& initialLineConstraints, bool isFirstLine);
    void commitFloats(const LineCandidate&, CommitIntrusiveFloatsOnly = CommitIntrusiveFloatsOnly::No);
    Result handleFloatsAndInlineContent(LineBreaker&, const InlineItemRange& needsLayoutRange, const LineCandidate&);
    size_t rebuildLine(const InlineItemRange& needsLayoutRange);
    void commitPartialContent(const LineBreaker::RunList&, const LineBreaker::Result::PartialTrailingContent&);
    void initialize(const UsedConstraints&);
    struct CommittedContent {
        size_t inlineItemCount { 0 };
        Optional <LineContent::PartialContent> partialTrailingContent;
    };
    CommittedContent placeInlineContent(const InlineItemRange&, Optional<unsigned> partialLeadingContentLength);
    InlineItemRange close(const InlineItemRange& needsLayoutRange, const CommittedContent&);

    InlineLayoutUnit inlineItemWidth(const InlineItem&, InlineLayoutUnit contentLogicalLeft) const;
    bool isLastLineWithInlineContent(const InlineItemRange& lineRange, size_t lastInlineItemIndex, bool hasPartialTrailingContent) const;

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    const ContainerBox& root() const { return m_formattingContextRoot; }
    const LayoutState& layoutState() const;

    const InlineFormattingContext& m_inlineFormattingContext;
    const FloatingContext& m_floatingContext;
    const ContainerBox& m_formattingContextRoot;
    Line m_line;
    const InlineItems& m_inlineItems;
    LineContent::FloatList m_floats;
    Optional<InlineTextItem> m_partialLeadingTextItem;
    const InlineItem* m_lastWrapOpportunityItem { nullptr };
    unsigned m_successiveHyphenatedLineCount { 0 };
};

}
}
#endif
