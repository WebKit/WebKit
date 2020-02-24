/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "LayoutUnits.h"

namespace WebCore {

class RenderStyle;

namespace Layout {

class InlineItem;
class InlineTextItem;
struct ContinuousContent;
struct WrappedTextContent;

class LineBreaker {
public:
    struct PartialRun {
        unsigned length { 0 };
        InlineLayoutUnit logicalWidth { 0 };
        bool needsHyphen { false };
    };
    enum class IsEndOfLine { No, Yes };
    struct Result {
        enum class Action {
            Keep, // Keep content on the current line.
            Split, // Partial content is on the current line.
            Push, // Content is pushed to the next line.
            RevertToLastWrapOpportunity // The current content overflows and can't get wrapped. The line needs to be reverted back to the last line wrapping opportunity.
        };
        struct PartialTrailingContent {
            size_t trailingRunIndex { 0 };
            Optional<PartialRun> partialRun; // nullopt partial run means the trailing run is a complete run.
        };

        Action action { Action::Keep };
        IsEndOfLine isEndOfLine { IsEndOfLine::No };
        Optional<PartialTrailingContent> partialTrailingContent { };
        const InlineItem* lastWrapOpportunityItem { nullptr };
    };

    struct Run {
        Run(const InlineItem&, InlineLayoutUnit);
        Run(const Run&);
        Run& operator=(const Run&);

        const InlineItem& inlineItem;
        InlineLayoutUnit logicalWidth { 0 };
    };
    using RunList = Vector<Run, 3>;

    struct LineStatus {
        InlineLayoutUnit availableWidth { 0 };
        InlineLayoutUnit collapsibleWidth { 0 };
        bool lineHasFullyCollapsibleTrailingRun { false };
        bool lineIsEmpty { true };
    };
    Result shouldWrapInlineContent(const RunList& candidateRuns, InlineLayoutUnit candidateContentLogicalWidth, const LineStatus&);

    void setHyphenationDisabled() { n_hyphenationIsDisabled = true; }

private:
    // This struct represents the amount of content committed to line breaking at a time e.g.
    // text content <span>span1</span>between<span>span2</span>
    // [text][ ][content][ ][container start][span1][container end][between][container start][span2][container end]
    // -> content chunks ->
    // [text]
    // [ ]
    // [content]
    // [container start][span1][container end][between][container start][span2][container end]
    // see https://drafts.csswg.org/css-text-3/#line-break-details
    Optional<WrappedTextContent> wrapTextContent(const RunList&, const LineStatus&) const;
    Result tryWrappingInlineContent(const RunList&, InlineLayoutUnit candidateContentLogicalWidth, const LineStatus&) const;
    Optional<PartialRun> tryBreakingTextRun(const Run& overflowRun, InlineLayoutUnit availableWidth) const;

    enum class WordBreakRule {
        NoBreak,
        AtArbitraryPosition,
        OnlyHyphenationAllowed
    };
    WordBreakRule wordBreakBehavior(const RenderStyle&) const;
    bool shouldKeepEndOfLineWhitespace(const ContinuousContent&) const;
    bool isContentWrappingAllowed(const ContinuousContent&) const;

    bool n_hyphenationIsDisabled { false };
    bool m_hasWrapOpportunityAtPreviousPosition { false };
};

inline LineBreaker::Run::Run(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
    : inlineItem(inlineItem)
    , logicalWidth(logicalWidth)
{
}

inline LineBreaker::Run::Run(const Run& other)
    : inlineItem(other.inlineItem)
    , logicalWidth(other.logicalWidth)
{
}

}
}
#endif
