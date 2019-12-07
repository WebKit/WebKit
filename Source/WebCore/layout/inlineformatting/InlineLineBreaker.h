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
    struct BreakingContext {
        enum class ContentWrappingRule {
            Keep, // Keep content on the current line.
            Split, // Partial content is on the current line.
            Push // Content is pushed to the next line.
        };
        ContentWrappingRule contentWrappingRule;
        struct PartialTrailingContent {
            unsigned runIndex { 0 };
            unsigned length { 0 };
            InlineLayoutUnit logicalWidth;
            bool needsHyphen { false };
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
        void trim(unsigned newSize);

        static bool isAtContentBoundary(const InlineItem&, const Content&);

        struct Run {
            const InlineItem& inlineItem;
            InlineLayoutUnit logicalWidth;
        };
        using RunList = Vector<Run, 30>;

        RunList& runs() { return m_continousRuns; }
        const RunList& runs() const { return m_continousRuns; }
        bool isEmpty() const { return m_continousRuns.isEmpty(); }
        bool hasTextContentOnly() const;
        bool hasNonContentRunsOnly() const;
        unsigned size() const { return m_continousRuns.size(); }
        InlineLayoutUnit width() const { return m_width; }
        InlineLayoutUnit nonTrimmableWidth() const { return m_width - m_trailingTrimmableContent.width; }

        bool hasTrailingTrimmableContent() const { return !!m_trailingTrimmableContent.width; }
        bool isTrailingContentFullyTrimmable() const { return m_trailingTrimmableContent.isFullyTrimmable; }

    private:
        RunList m_continousRuns;
        struct TrailingTrimmableContent {
            void reset();

            bool isFullyTrimmable { false };
            InlineLayoutUnit width;
        };
        TrailingTrimmableContent m_trailingTrimmableContent;
        InlineLayoutUnit m_width;
    };

    struct LineStatus {
        InlineLayoutUnit availableWidth;
        InlineLayoutUnit trimmableWidth;
        bool lineHasFullyTrimmableTrailingRun;
        bool lineIsEmpty;
    };
    BreakingContext breakingContextForInlineContent(const Content& candidateRuns, const LineStatus&);
    bool shouldWrapFloatBox(InlineLayoutUnit floatLogicalWidth, InlineLayoutUnit availableWidth, bool lineIsEmpty);

    void setHyphenationDisabled() { n_hyphenationIsDisabled = true; }

private:
    Optional<BreakingContext::PartialTrailingContent> wordBreakingBehavior(const Content::RunList&, InlineLayoutUnit availableWidth) const;
    struct LeftSide {
        unsigned length { 0 };
        InlineLayoutUnit logicalWidth;
        bool needsHyphen { false };
    };
    Optional<LeftSide> tryBreakingTextRun(const Content::Run& overflowRun, InlineLayoutUnit availableWidth) const;

    bool n_hyphenationIsDisabled { false };
};

}
}
#endif
