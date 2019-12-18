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
namespace Layout {

class InlineItem;
class InlineTextItem;

class LineBreaker {
public:
    struct PartialRun {
        unsigned length { 0 };
        InlineLayoutUnit logicalWidth { 0 };
        bool needsHyphen { false };
    };
    struct BreakingContext {
        enum class ContentWrappingRule {
            Keep, // Keep content on the current line.
            Split, // Partial content is on the current line.
            Push // Content is pushed to the next line.
        };
        ContentWrappingRule contentWrappingRule;
        struct PartialTrailingContent {
            unsigned triailingRunIndex { 0 };
            Optional<PartialRun> partialRun; // Empty partial run means the trailing run is a complete run.
        };
        Optional<PartialTrailingContent> partialTrailingContent;
    };

    // This struct represents the amount of content committed to line breaking at a time e.g.
    // text content <span>span1</span>between<span>span2</span>
    // [text][ ][content][ ][container start][span1][container end][between][container start][span2][container end]
    // -> content chunks ->
    // [text]
    // [ ]
    // [content]
    // [container start][span1][container end][between][container start][span2][container end]
    // see https://drafts.csswg.org/css-text-3/#line-break-details
    struct Content {
        void append(const InlineItem&, InlineLayoutUnit logicalWidth);
        void reset();
        void shrink(unsigned newSize);

        static Optional<unsigned> lastSoftWrapOpportunity(const InlineItem&, const Content& priorContent);

        struct Run {
            const InlineItem& inlineItem;
            InlineLayoutUnit logicalWidth { 0 };
        };
        using RunList = Vector<Run, 30>;

        RunList& runs() { return m_continousRuns; }
        const RunList& runs() const { return m_continousRuns; }
        bool isEmpty() const { return m_continousRuns.isEmpty(); }
        bool hasTextContentOnly() const;
        bool isVisuallyEmptyWhitespaceContentOnly() const;
        bool hasNonContentRunsOnly() const;
        unsigned size() const { return m_continousRuns.size(); }
        InlineLayoutUnit width() const { return m_width; }
        InlineLayoutUnit nonCollapsibleWidth() const { return m_width - m_trailingCollapsibleContent.width; }

        bool hasTrailingCollapsibleContent() const { return !!m_trailingCollapsibleContent.width; }
        bool isTrailingContentFullyCollapsible() const { return m_trailingCollapsibleContent.isFullyCollapsible; }

        Optional<unsigned> firstTextRunIndex() const;

    private:
        RunList m_continousRuns;
        struct TrailingCollapsibleContent {
            void reset();

            bool isFullyCollapsible { false };
            InlineLayoutUnit width { 0 };
        };
        TrailingCollapsibleContent m_trailingCollapsibleContent;
        InlineLayoutUnit m_width { 0 };
    };

    struct LineStatus {
        InlineLayoutUnit availableWidth { 0 };
        InlineLayoutUnit collapsibleWidth { 0 };
        bool lineHasFullyCollapsibleTrailingRun { false };
        bool lineIsEmpty { true };
    };
    BreakingContext breakingContextForInlineContent(const Content& candidateRuns, const LineStatus&);
    bool shouldWrapFloatBox(InlineLayoutUnit floatLogicalWidth, InlineLayoutUnit availableWidth, bool lineIsEmpty);

    void setHyphenationDisabled() { n_hyphenationIsDisabled = true; }

private:
    struct WrappedTextContent {
        unsigned trailingRunIndex { 0 };
        bool contentOverflows { false };
        Optional<PartialRun> partialTrailingRun;
    };
    Optional<WrappedTextContent> wrapTextContent(const Content::RunList&, const LineStatus&) const;
    Optional<PartialRun> tryBreakingTextRun(const Content::Run& overflowRun, InlineLayoutUnit availableWidth, bool lineIsEmpty) const;

    enum class WordBreakRule {
        NoBreak,
        AtArbitraryPosition,
        OnlyHyphenationAllowed
    };
    WordBreakRule wordBreakBehavior(const RenderStyle&, bool lineIsEmpty) const;

    bool n_hyphenationIsDisabled { false };
};

}
}
#endif
