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

#include "config.h"
#include "InlineLineBreaker.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "Hyphenation.h"
#include "InlineItem.h"
#include "InlineTextItem.h"

namespace WebCore {
namespace Layout {

LineBreaker::BreakingContext LineBreaker::breakingContextForInlineContent(const Vector<LineLayout::Run>& runs, LayoutUnit logicalWidth, LayoutUnit availableWidth, bool lineIsEmpty)
{
    if (logicalWidth <= availableWidth)
        return { BreakingContext::ContentBreak::Keep, { } };

    auto isTextContent = [](auto& runs) {
        // <span>text</span> is considered a text run even with the [container start][container end] inline items.
        for (auto& run : runs) {
            auto& inlineItem = run.inlineItem;
            // Skip "typeless" inline items.
            if (inlineItem.isContainerStart() || inlineItem.isContainerEnd())
                continue;
            return inlineItem.isText();
        }
        return false;
    };
    if (isTextContent(runs))
        return wordBreakingBehavior(runs, availableWidth, lineIsEmpty);

    // First non-text inline content always stays on line.
    return { lineIsEmpty ? BreakingContext::ContentBreak::Keep : BreakingContext::ContentBreak::Wrap, { } };
}

bool LineBreaker::shouldWrapFloatBox(LayoutUnit floatLogicalWidth, LayoutUnit availableWidth, bool lineIsEmpty)
{
    return !lineIsEmpty && floatLogicalWidth > availableWidth;
}

LineBreaker::BreakingContext LineBreaker::wordBreakingBehavior(const Vector<LineLayout::Run>& runs, LayoutUnit availableWidth, bool lineIsEmpty) const
{
    // Check where the overflow occurs and use the corresponding style to figure out the breaking behaviour.
    // <span style="word-break: normal">first</span><span style="word-break: break-all">second</span><span style="word-break: normal">third</span>
    LayoutUnit runsWidth = 0;
    for (unsigned i = 0; i < runs.size(); ++i) {
        auto& run = runs[i];
        ASSERT(run.inlineItem.isText() || run.inlineItem.isContainerStart() || run.inlineItem.isContainerEnd());
        runsWidth += run.logicalWidth;
        if (runsWidth <= availableWidth)
            continue;
        // Let's find the first breaking opportunity starting from this overflown inline item.
        if (!run.inlineItem.isText()) {
            // Can't split horizontal spacing -> e.g. <span style="padding-right: 100px;">textcontent</span>, if the [container end] is the overflown inline item
            // we need to check if there's another inline item beyond the [container end] to split.
            continue;
        }
        // Do not even try to wrap 'pre' and 'no-wrap' content.
        auto whitespace = run.inlineItem.style().whiteSpace();
        if (whitespace == WhiteSpace::Pre || whitespace == WhiteSpace::NoWrap)
            continue;
        // At this point the available width can very well be negative e.g. when some part of the continuous text content can not be broken into parts ->
        // <span style="word-break: keep-all">textcontentwithnobreak</span><span>textcontentwithyesbreak</span>
        // When the first span computes longer than the available space, by the time we get to the second span, the adjusted available space becomes negative.
        auto adjustedAvailableWidth = std::max(LayoutUnit { }, availableWidth - runsWidth + run.logicalWidth);
        if (auto splitLengthAndWidth = tryBreakingTextRun(run, adjustedAvailableWidth))
            return { BreakingContext::ContentBreak::Split, BreakingContext::TrailingPartialContent { i, splitLengthAndWidth->length, splitLengthAndWidth->leftLogicalWidth } };
    }
    // We did not manage to break in this sequence of runs.
    return { lineIsEmpty ? BreakingContext::ContentBreak::Keep : BreakingContext::ContentBreak::Wrap, { } };
}

Optional<LineBreaker::SplitLengthAndWidth> LineBreaker::tryBreakingTextRun(const LineLayout::Run overflowRun, LayoutUnit availableWidth) const
{
    ASSERT(overflowRun.inlineItem.isText());
    auto breakWords = overflowRun.inlineItem.style().wordBreak();
    if (breakWords == WordBreak::KeepAll)
        return { };
    auto& inlineTextItem = downcast<InlineTextItem>(overflowRun.inlineItem);
    if (breakWords == WordBreak::BreakAll) {
        // FIXME: Pass in the content logical left to be able to measure tabs.
        auto splitData = TextUtil::split(inlineTextItem.layoutBox(), inlineTextItem.start(), inlineTextItem.length(), overflowRun.logicalWidth, availableWidth, { });
        return SplitLengthAndWidth { splitData.length, splitData.logicalWidth };
    }
    // FIXME: Find first soft wrap opportunity (e.g. hyphenation)
    return { };
}

}
}
#endif
