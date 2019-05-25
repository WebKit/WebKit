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
#include "LayoutState.h"

namespace WebCore {
namespace Layout {

LineBreaker::LineBreaker(const LayoutState& layoutState)
    : m_layoutState(layoutState)
{
}

LineBreaker::BreakingContext LineBreaker::breakingContext(InlineItem& inlineItem, LineContext lineContext)
{
    inlineItem.setWidth(runWidth(inlineItem, lineContext.logicalLeft));
    // First content always stays on line.
    if (lineContext.isEmpty || inlineItem.width() <= lineContext.availableWidth)
        return { BreakingBehavior::Keep, isAtBreakingOpportunity(inlineItem) };

    if (is<InlineTextItem>(inlineItem))
        return { wordBreakingBehavior(downcast<InlineTextItem>(inlineItem), lineContext.isEmpty), isAtBreakingOpportunity(inlineItem) };

    // Wrap non-text boxes to the next line unless we can trim trailing whitespace.
    auto availableWidth = lineContext.availableWidth + lineContext.trimmableWidth;
    if (inlineItem.width() <= availableWidth)
        return { BreakingBehavior::Keep, isAtBreakingOpportunity(inlineItem) };
    return { BreakingBehavior::Wrap, isAtBreakingOpportunity(inlineItem) };
}

LineBreaker::BreakingBehavior LineBreaker::wordBreakingBehavior(const InlineTextItem& inlineItem, bool lineIsEmpty) const
{
    // Word breaking behaviour:
    // 1. Whitesapce collapse on -> push whitespace to next line.
    // 2. Whitespace collapse off -> whitespace is split where possible.
    // 3. Non-whitespace -> first run on the line -> either split or kept on the line. (depends on overflow-wrap)
    // 4. Non-whitespace -> already content on the line -> either gets split (word-break: break-all) or gets pushed to the next line.
    // (Hyphenate when possible)
    // 5. Non-text type -> next line
    auto& style = inlineItem.style();

    if (inlineItem.isWhitespace())
        return style.collapseWhiteSpace() ? BreakingBehavior::Wrap : BreakingBehavior::Break;

    auto shouldHypenate = !m_hyphenationIsDisabled && style.hyphens() == Hyphens::Auto && canHyphenate(style.locale());
    if (shouldHypenate)
        return BreakingBehavior::Break;

    if (style.autoWrap()) {
        // Break any word
        if (style.wordBreak() == WordBreak::BreakAll)
            return BreakingBehavior::Break;

        // Break first run on line.
        if (lineIsEmpty && style.breakWords() && style.preserveNewline())
            return BreakingBehavior::Break;
    }

    // Non-breakable non-whitespace run.
    return lineIsEmpty ? BreakingBehavior::Keep : BreakingBehavior::Wrap;
}

LayoutUnit LineBreaker::runWidth(const InlineItem& inlineItem, LayoutUnit contentLogicalLeft) const
{
    if (inlineItem.isLineBreak())
        return 0;

    if (is<InlineTextItem>(inlineItem))
        return textWidth(downcast<InlineTextItem>(inlineItem), contentLogicalLeft);

    auto& layoutBox = inlineItem.layoutBox();
    ASSERT(m_layoutState.hasDisplayBox(layoutBox));
    auto& displayBox = m_layoutState.displayBoxForLayoutBox(layoutBox);

    if (inlineItem.isContainerStart())
        return displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0);

    if (inlineItem.isContainerEnd())
        return displayBox.marginEnd() + displayBox.borderRight() + displayBox.paddingRight().valueOr(0);

    if (inlineItem.isFloat())
        return displayBox.marginBoxWidth();

    return displayBox.width();
}

bool LineBreaker::isAtBreakingOpportunity(const InlineItem& inlineItem)
{
    if (is<InlineTextItem>(inlineItem))
        return downcast<InlineTextItem>(inlineItem).isWhitespace();
    return !inlineItem.isFloat() && !inlineItem.isContainerStart() && !inlineItem.isContainerEnd();
}

LayoutUnit LineBreaker::textWidth(const InlineTextItem& inlineTextItem, LayoutUnit contentLogicalLeft) const
{
    auto end = inlineTextItem.isCollapsed() ? inlineTextItem.start() + 1 : inlineTextItem.end();
    return TextUtil::width(downcast<InlineBox>(inlineTextItem.layoutBox()), inlineTextItem.start(), end, contentLogicalLeft);
}
}
}
#endif
