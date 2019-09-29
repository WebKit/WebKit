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
#include "InlineLineBreaker.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class LineLayout {
public:
    struct LineInput;
    LineLayout(const InlineFormattingContext&, const LineInput&);

    struct LineContent;
    LineContent layout();

    struct IndexAndRange {
        unsigned index { 0 };
        struct Range {
            unsigned start { 0 };
            unsigned length { 0 };
        };
        Optional<Range> partialContext;
    };

    struct LineInput {
        LineInput(const Line::InitialConstraints&, TextAlignMode, IndexAndRange firstToProcess, const InlineItems&);
        LineInput(const Line::InitialConstraints&, IndexAndRange firstToProcess, const InlineItems&);

        Line::InitialConstraints initialConstraints;
        TextAlignMode horizontalAlignment;
        // FIXME Alternatively we could just have a second pass with vertical positioning (preferred width computation opts out) 
        Line::SkipAlignment skipAlignment { Line::SkipAlignment::No };
        IndexAndRange firstInlineItem;
        const InlineItems& inlineItems;
        Optional<LayoutUnit> floatMinimumLogicalBottom;
    };

    struct LineContent {
        Optional<IndexAndRange> lastCommitted;
        Vector<WeakPtr<InlineItem>> floats;
        const Line::RunList runList;
        const LineBox lineBox;
    };

private:
    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    enum class IsEndOfLine { No, Yes };
    IsEndOfLine placeInlineItem(const InlineItem&);
    void commitPendingContent();
    LineContent close();
    
    struct UncommittedContent {
        struct Run {
            const InlineItem& inlineItem;
            LayoutUnit logicalWidth;
        };
        void add(const InlineItem&, LayoutUnit logicalWidth);
        void reset();

        Vector<Run> runs() { return m_uncommittedRuns; }
        bool isEmpty() const { return m_uncommittedRuns.isEmpty(); }
        unsigned size() const { return m_uncommittedRuns.size(); }
        LayoutUnit width() const { return m_width; }

    private:
        Vector<Run> m_uncommittedRuns;
        LayoutUnit m_width;
    };

    const InlineFormattingContext& m_inlineFormattingContext;
    const LineInput& m_lineInput;
    Line m_line;
    LineBreaker m_lineBreaker;
    bool m_lineHasIntrusiveFloat { false };
    UncommittedContent m_uncommittedContent;
    unsigned m_committedInlineItemCount { 0 };
    Vector<WeakPtr<InlineItem>> m_floats;
    std::unique_ptr<InlineTextItem> m_leadingPartialInlineTextItem;
    std::unique_ptr<InlineTextItem> m_trailingPartialInlineTextItem;
};

}
}
#endif
