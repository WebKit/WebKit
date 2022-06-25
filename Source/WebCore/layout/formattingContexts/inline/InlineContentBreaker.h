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

#include "FormattingConstraints.h"
#include "LayoutUnits.h"
#include "RenderStyle.h"

namespace WebCore {
namespace Layout {

class InlineItem;
class InlineTextItem;
struct CandidateTextRunForBreaking;

class InlineContentBreaker {
public:
    InlineContentBreaker(std::optional<IntrinsicWidthMode>);

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
        std::optional<InlineLayoutUnit> leadingCollapsibleWidth() const { return m_leadingCollapsibleWidth; }
        std::optional<InlineLayoutUnit> trailingCollapsibleWidth() const { return m_trailingCollapsibleWidth; }
        bool hasCollapsibleContent() const { return trailingCollapsibleWidth() || leadingCollapsibleWidth(); }
        bool isFullyCollapsible() const;
        bool isHangingContent() const { return m_trailingHangingContentWidth && logicalWidth() == *m_trailingHangingContentWidth; }

        void append(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);
        void append(const InlineTextItem&, const RenderStyle&, InlineLayoutUnit logicalWidth, std::optional<InlineLayoutUnit> collapsibleWidth);
        void append(const InlineTextItem&, const RenderStyle&, InlineLayoutUnit hangingWidth);
        void reset();

        struct Run {
            Run(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);
            Run(const Run&);
            Run& operator=(const Run&);

            const InlineItem& inlineItem;
            const RenderStyle& style;
            InlineLayoutUnit logicalWidth { 0 };
        };
        using RunList = Vector<Run, 3>;
        const RunList& runs() const { return m_runs; }

    private:
        void appendToRunList(const InlineItem&, const RenderStyle&, InlineLayoutUnit logicalWidth);
        void resetTrailingWhitespace();

        RunList m_runs;
        InlineLayoutUnit m_logicalWidth { 0 };
        std::optional<InlineLayoutUnit> m_leadingCollapsibleWidth { };
        std::optional<InlineLayoutUnit> m_trailingCollapsibleWidth { };
        std::optional<InlineLayoutUnit> m_trailingHangingContentWidth { };
    };

    struct LineStatus {
        InlineLayoutUnit contentLogicalRight { 0 };
        InlineLayoutUnit availableWidth { 0 };
        // Both of these types of trailing content may be ignored when checking for content fit.
        InlineLayoutUnit collapsibleOrHangingWidth { 0 };
        std::optional<InlineLayoutUnit> trailingSoftHyphenWidth;
        bool hasFullyCollapsibleTrailingContent { false };
        bool hasContent { false };
        bool hasWrapOpportunityAtPreviousPosition { false };
    };
    Result processInlineContent(const ContinuousContent&, const LineStatus&);
    void setHyphenationDisabled() { n_hyphenationIsDisabled = true; }

    static bool isWrappingAllowed(const ContinuousContent::Run&);

private:
    Result processOverflowingContent(const ContinuousContent&, const LineStatus&) const;

    struct OverflowingTextContent {
        size_t runIndex { 0 }; // Overflowing run index. There's always an overflowing run.
        struct BreakingPosition {
            size_t runIndex { 0 };
            struct TrailingContent {
                // Trailing content is either the run's left side (when we break the run somewhere in the middle) or the previous run.
                // Sometimes the breaking position is at the very beginning of the first run, so there's no trailing run at all.
                bool overflows { false };
                std::optional<InlineContentBreaker::PartialRun> partialRun { };
            };
            std::optional<TrailingContent> trailingContent { };
        };
        std::optional<BreakingPosition> breakingPosition { }; // Where we actually break this overflowing content.
    };
    OverflowingTextContent processOverflowingContentWithText(const ContinuousContent&, const LineStatus&) const;
    std::optional<PartialRun> tryBreakingTextRun(const ContinuousContent::RunList& runs, const CandidateTextRunForBreaking&, InlineLayoutUnit availableWidth, bool lineHasWrapOpportunityAtPreviousPosition) const;
    std::optional<OverflowingTextContent::BreakingPosition> tryBreakingOverflowingRun(const LineStatus&, const ContinuousContent::RunList&, size_t overflowingRunIndex, InlineLayoutUnit nonOverflowingContentWidth) const;
    std::optional<OverflowingTextContent::BreakingPosition> tryBreakingPreviousNonOverflowingRuns(const LineStatus&, const ContinuousContent::RunList&, size_t overflowingRunIndex, InlineLayoutUnit nonOverflowingContentWidth) const;
    std::optional<OverflowingTextContent::BreakingPosition> tryBreakingNextOverflowingRuns(const LineStatus&, const ContinuousContent::RunList&, size_t overflowingRunIndex, InlineLayoutUnit nonOverflowingContentWidth) const;

    enum class WordBreakRule {
        AtArbitraryPositionWithinWords = 1 << 0,
        AtArbitraryPosition            = 1 << 1,
        AtHyphenationOpportunities     = 1 << 2
    };
    OptionSet<WordBreakRule> wordBreakBehavior(const RenderStyle&, bool hasWrapOpportunityAtPreviousPosition) const;
    bool isInIntrinsicWidthMode() const { return !!m_intrinsicWidthMode; }

    std::optional<IntrinsicWidthMode> m_intrinsicWidthMode;
    bool n_hyphenationIsDisabled { false };
};

inline InlineContentBreaker::ContinuousContent::Run::Run(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
    : inlineItem(inlineItem)
    , style(style)
    , logicalWidth(logicalWidth)
{
}

inline InlineContentBreaker::ContinuousContent::Run::Run(const Run& other)
    : inlineItem(other.inlineItem)
    , style(other.style)
    , logicalWidth(other.logicalWidth)
{
}

inline bool InlineContentBreaker::ContinuousContent::isFullyCollapsible() const
{
    auto collapsibleWidth = std::optional<InlineLayoutUnit> { };
    if (m_leadingCollapsibleWidth)
        collapsibleWidth = *m_leadingCollapsibleWidth;
    if (m_trailingCollapsibleWidth)
        collapsibleWidth = collapsibleWidth.value_or(0.f) + *m_trailingCollapsibleWidth;
    return collapsibleWidth && *collapsibleWidth == logicalWidth();
}

}
}
#endif
