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

#include "LayoutUnit.h"

namespace WebCore {
namespace Layout {

class InlineItem;
class InlineTextItem;

class LineBreaker {
public:
    struct BreakingContext {
        enum class ContentBreak { Keep, Split, Wrap };
        ContentBreak contentBreak;
        struct TrailingPartialContent {
            unsigned runIndex { 0 };
            unsigned length { 0 };
            LayoutUnit logicalWidth;
            bool hasHyphen { false };
        };
        Optional<TrailingPartialContent> trailingPartialContent;
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
        void append(const InlineItem&, LayoutUnit logicalWidth);
        void reset();
        void trim(unsigned newSize);

        static bool isAtContentBoundary(const InlineItem&, const Content&);

        struct Run {
            const InlineItem& inlineItem;
            LayoutUnit logicalWidth;
        };
        using RunList = Vector<Run, 30>;

        RunList& runs() { return m_continousRuns; }
        const RunList& runs() const { return m_continousRuns; }
        bool isEmpty() const { return m_continousRuns.isEmpty(); }
        bool hasTextContentOnly() const;
        unsigned size() const { return m_continousRuns.size(); }
        LayoutUnit width() const { return m_width; }
        LayoutUnit nonTrimmableWidth() const { return m_width - m_trailingTrimmableContent.width; }

        bool hasTrailingTrimmableContent() const { return !!m_trailingTrimmableContent.width; }
        bool isTrailingContentFullyTrimmable() const { return m_trailingTrimmableContent.isFullyTrimmable; }

    private:
        RunList m_continousRuns;
        struct TrailingTrimmableContent {
            void reset();

            bool isFullyTrimmable { false };
            LayoutUnit width;
        };
        TrailingTrimmableContent m_trailingTrimmableContent;
        LayoutUnit m_width;
    };

    BreakingContext breakingContextForInlineContent(const Content&, LayoutUnit availableWidth, bool lineHasFullyTrimmableTrailingContent, bool lineIsEmpty);
    bool shouldWrapFloatBox(LayoutUnit floatLogicalWidth, LayoutUnit availableWidth, bool lineIsEmpty);

    void setHyphenationDisabled() { n_hyphenationIsDisabled = true; }

private:
    Optional<BreakingContext::TrailingPartialContent> wordBreakingBehavior(const Content::RunList&, LayoutUnit availableWidth) const;
    struct LeftSide {
        unsigned length { 0 };
        LayoutUnit logicalWidth;
        bool hasHyphen { false };
    };
    Optional<LeftSide> tryBreakingTextRun(const Content::Run& overflowRun, LayoutUnit availableWidth) const;

    bool n_hyphenationIsDisabled { false };
};

}
}
#endif
