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

LineBreaker::BreakingBehavior LineBreaker::breakingContext(const Vector<LineLayout::Run>& runs, LayoutUnit logicalWidth, LayoutUnit availableWidth, bool lineIsEmpty)
{
    // First content always stays on line.
    if (lineIsEmpty || logicalWidth <= availableWidth)
        return BreakingBehavior::Keep;

    // FIXME: Look inside the list to find out whether it requires text content related line breaking (e.g <span>text</span> <- the first inline item here is a 'container start' but the content is text only).
    auto& inlineItem = runs.first().inlineItem;
    if (is<InlineTextItem>(inlineItem))
        return wordBreakingBehavior(runs, lineIsEmpty);

    return BreakingBehavior::Wrap;
}

LineBreaker::BreakingBehavior LineBreaker::breakingContextForFloat(LayoutUnit floatLogicalWidth, LayoutUnit availableWidth, bool lineIsEmpty)
{
    return (lineIsEmpty || floatLogicalWidth <= availableWidth) ? BreakingBehavior::Keep : BreakingBehavior::Wrap;
}

LineBreaker::BreakingBehavior LineBreaker::wordBreakingBehavior(const Vector<LineLayout::Run>& runs, bool lineIsEmpty) const
{
    // FIXME: Check where the overflow occurs and use the corresponding style to figure out the breaking behaviour.
    // <span style="word-break: normal">first</span><span style="word-break: break-all">second</span><span style="word-break: normal">third</span>
    auto& inlineItem = downcast<InlineTextItem>(runs.first().inlineItem);

    // Word breaking behaviour:
    // 1. Whitesapce collapse on -> push whitespace to next line.
    // 2. Whitespace collapse off -> whitespace is split where possible.
    // 3. Non-whitespace -> first run on the line -> either split or kept on the line. (depends on overflow-wrap)
    // 4. Non-whitespace -> already content on the line -> either gets split (word-break: break-all) or gets pushed to the next line.
    // (Hyphenate when possible)
    // 5. Non-text type -> next line
    auto& style = inlineItem.style();

    if (inlineItem.isWhitespace())
        return style.collapseWhiteSpace() ? BreakingBehavior::Wrap : BreakingBehavior::Split;

    auto shouldHypenate = !m_hyphenationIsDisabled && style.hyphens() == Hyphens::Auto && canHyphenate(style.locale());
    if (shouldHypenate)
        return BreakingBehavior::Split;

    if (style.autoWrap()) {
        // Break any word
        if (style.wordBreak() == WordBreak::BreakAll)
            return BreakingBehavior::Split;

        // Break first run on line.
        if (lineIsEmpty && style.breakWords() && style.preserveNewline())
            return BreakingBehavior::Split;
    }

    // Non-breakable non-whitespace run.
    return lineIsEmpty ? BreakingBehavior::Keep : BreakingBehavior::Wrap;
}

}
}
#endif
