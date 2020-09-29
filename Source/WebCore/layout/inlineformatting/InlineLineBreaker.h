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
struct TrailingTextContent;

class LineBreaker {
public:
    struct PartialRun {
        size_t length { 0 };
        InlineLayoutUnit logicalWidth { 0 };
        bool needsHyphen { false };
    };
    enum class IsEndOfLine { No, Yes };
    struct Result {
        enum class Action {
            Keep, // Keep content on the current line.
            Break, // Partial content is on the current line.
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

    // This struct represents the amount of continuous content committed to line breaking at a time (no in-between wrap opportunities).
    // e.g.
    // <div>text content <span>span1</span>between<span>span2</span></div>
    // [text][ ][content][ ][container start][span1][container end][between][container start][span2][container end]
    // continuous candidate content at a time:
    // 1. [text]
    // 2. [ ]
    // 3. [content]
    // 4. [ ]
    // 5. [container start][span1][container end][between][container start][span2][container end]
    // see https://drafts.csswg.org/css-text-3/#line-break-details
    struct CandidateContent {
        const RunList& runs;
        InlineLayoutUnit logicalLeft { 0 };
        InlineLayoutUnit logicalWidth { 0 };
        InlineLayoutUnit collapsibleTrailingWidth { 0 };
    };
    struct LineStatus {
        InlineLayoutUnit availableWidth { 0 };
        InlineLayoutUnit collapsibleWidth { 0 };
        bool lineHasFullyCollapsibleTrailingRun { false };
        bool lineIsEmpty { true };
    };
    Result processInlineContent(const CandidateContent&, const LineStatus&);

    void setHyphenationDisabled() { n_hyphenationIsDisabled = true; }

private:
    Result processOverflowingContent(const CandidateContent&, const LineStatus&) const;
    Optional<TrailingTextContent> processOverflowingTextContent(const ContinuousContent&, const LineStatus&) const;
    Optional<PartialRun> tryBreakingTextRun(const Run& overflowRun, InlineLayoutUnit logicalLeft, InlineLayoutUnit availableWidth) const;

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
