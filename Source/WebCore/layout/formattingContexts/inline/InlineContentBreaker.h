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
struct OverflowingTextContent;

class InlineContentBreaker {
public:
    struct PartialRun {
        size_t length { 0 };
        InlineLayoutUnit logicalWidth { 0 };
        std::optional<InlineLayoutUnit> hyphenWidth { };
    };
    enum class IsEndOfLine { No, Yes };
    struct Result {
        enum class Action {
            Keep, // Keep content on the current line.
            Break, // Partial content is on the current line.
            Wrap, // Content is wrapped to the next line.
            WrapWithHyphen, // Content is wrapped to the next line and the current line ends with a visible hyphen.
            // The current content overflows and can't get broken up into smaller bits.
            RevertToLastWrapOpportunity, // The content needs to be reverted back to the last wrap opportunity.
            RevertToLastNonOverflowingWrapOpportunity // The content needs to be reverted back to a wrap opportunity that still fits the line.
        };
        struct PartialTrailingContent {
            size_t trailingRunIndex { 0 };
            std::optional<PartialRun> partialRun; // nullopt partial run means the trailing run is a complete run.
        };
        Action action { Action::Keep };
        IsEndOfLine isEndOfLine { IsEndOfLine::No };
        std::optional<PartialTrailingContent> partialTrailingContent { };
        const InlineItem* lastWrapOpportunityItem { nullptr };
    };

    // This struct represents the amount of continuous content committed to content breaking at a time (no in-between wrap opportunities).
    // e.g.
    // <div>text content <span>span1</span>between<span>span2</span></div>
    // [text][ ][content][ ][inline box start][span1][inline box end][between][inline box start][span2][inline box end]
    // continuous candidate content at a time:
    // 1. [text]
    // 2. [ ]
    // 3. [content]
    // 4. [ ]
    // 5. [inline box start][span1][inline box end][between][inline box start][span2][inline box end]
    // see https://drafts.csswg.org/css-text-3/#line-break-details
    struct ContinuousContent {
        InlineLayoutUnit logicalWidth() const { return m_logicalWidth; }
        InlineLayoutUnit collapsibleLogicalWidth() const { return m_collapsibleLogicalWidth; }
        InlineLayoutUnit nonCollapsibleLogicalWidth() const { return logicalWidth() - collapsibleLogicalWidth(); }
        bool hasTrailingCollapsibleContent() const { return !!collapsibleLogicalWidth(); }
        bool isFullyCollapsible() const { return logicalWidth() == collapsibleLogicalWidth(); }

        void append(const InlineItem&, InlineLayoutUnit logicalWidth, std::optional<InlineLayoutUnit> collapsibleWidth);
        void reset();

        struct Run {
            Run(const InlineItem&, InlineLayoutUnit logicalWidth);
            Run(const Run&);
            Run& operator=(const Run&);

            const InlineItem& inlineItem;
            InlineLayoutUnit logicalWidth { 0 };
        };
        using RunList = Vector<Run, 3>;
        const RunList& runs() const { return m_runs; }

    private:
        RunList m_runs;
        InlineLayoutUnit m_logicalWidth { 0 };
        InlineLayoutUnit m_collapsibleLogicalWidth { 0 };
    };

    struct LineStatus {
        InlineLayoutUnit contentLogicalRight { 0 };
        InlineLayoutUnit availableWidth { 0 };
        InlineLayoutUnit collapsibleWidth { 0 };
        std::optional<InlineLayoutUnit> trailingSoftHyphenWidth;
        bool hasFullyCollapsibleTrailingRun { false };
        bool hasContent { false };
        bool hasWrapOpportunityAtPreviousPosition { false };
    };
    Result processInlineContent(const ContinuousContent&, const LineStatus&);
    void setHyphenationDisabled() { n_hyphenationIsDisabled = true; }

    static bool isWrappingAllowed(const InlineItem&);

private:
    Result processOverflowingContent(const ContinuousContent&, const LineStatus&) const;
    OverflowingTextContent processOverflowingContentWithText(const ContinuousContent&, const LineStatus&) const;
    std::optional<PartialRun> tryBreakingTextRun(const ContinuousContent::Run& overflowRun, InlineLayoutUnit logicalLeft, std::optional<InlineLayoutUnit> availableWidth, bool hasWrapOpportunityAtPreviousPosition) const;

    enum class WordBreakRule {
        AtArbitraryPosition        = 1 << 0,
        AtHyphenationOpportunities = 1 << 1
    };
    OptionSet<WordBreakRule> wordBreakBehavior(const RenderStyle&, bool hasWrapOpportunityAtPreviousPosition) const;
    bool shouldKeepEndOfLineWhitespace(const ContinuousContent&) const;

    bool n_hyphenationIsDisabled { false };
};

inline InlineContentBreaker::ContinuousContent::Run::Run(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
    : inlineItem(inlineItem)
    , logicalWidth(logicalWidth)
{
}

inline InlineContentBreaker::ContinuousContent::Run::Run(const Run& other)
    : inlineItem(other.inlineItem)
    , logicalWidth(other.logicalWidth)
{
}

}
}
#endif
