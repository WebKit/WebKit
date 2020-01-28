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

#include "InlineLineBreaker.h"
#include "InlineLineBuilder.h"

namespace WebCore {
namespace Layout {

struct LineCandidateContent;

class LineLayoutContext {
public:
    LineLayoutContext(const InlineFormattingContext&, const Container& formattingContextRoot, const InlineItems&);

    struct LineContent {
        struct PartialContent {
            bool trailingContentNeedsHyphen { false };
            unsigned overflowContentLength { 0 };
        };
        Optional<unsigned> trailingInlineItemIndex;
        Optional<PartialContent> partialContent;
        Vector<const InlineItem*> floats;
        const LineBuilder::RunList runList;
        const LineBoxBuilder lineBox;
    };
    struct InlineItemRange {
        bool isEmpty() const { return start == end; }
        size_t start { 0 };
        size_t end { 0 };
    };
    LineContent layoutLine(LineBuilder&, const InlineItemRange, Optional<unsigned> partialLeadingContentLength);
    using FloatList = Vector<const InlineItem*>;

private:
    void nextContentForLine(LineCandidateContent&, unsigned inlineItemIndex, const InlineItemRange layoutRange, Optional<unsigned> overflowLength, InlineLayoutUnit currentLogicalRight);
    struct Result {
        LineBreaker::IsEndOfLine isEndOfLine { LineBreaker::IsEndOfLine::No };
        size_t committedCount { 0 };
        Optional <LineContent::PartialContent> partialContent { };
        const InlineItem* revertTo { nullptr };
    };
    Result tryAddingFloatItems(LineBuilder&, const FloatList&);
    Result tryAddingInlineItems(LineBreaker&, LineBuilder&, const LineCandidateContent&);
    void rebuildLineForRevert(LineBuilder&, const InlineItem& revertTo, const InlineItemRange layoutRange);
    void commitPartialContent(LineBuilder&, const LineBreaker::RunList&, const LineBreaker::Result::PartialTrailingContent&);
    LineContent close(LineBuilder&, const InlineItemRange layoutRange, unsigned committedInlineItemCount, Optional<LineContent::PartialContent>);

    InlineLayoutUnit inlineItemWidth(const InlineItem&, InlineLayoutUnit contentLogicalLeft) const;

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    const Container& root() const { return m_formattingContextRoot; }

    const InlineFormattingContext& m_inlineFormattingContext;
    const Container& m_formattingContextRoot;
    const InlineItems& m_inlineItems;
    FloatList m_floats;
    Optional<InlineTextItem> m_partialLeadingTextItem;
    Optional<InlineTextItem> m_partialTrailingTextItem;
    unsigned m_successiveHyphenatedLineCount { 0 };
};

}
}
#endif
