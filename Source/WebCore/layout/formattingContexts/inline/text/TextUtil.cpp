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
#include "TextUtil.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BreakLines.h"
#include "FontCascade.h"
#include "InlineTextItem.h"
#include "LayoutInlineTextBox.h"
#include "RenderBox.h"
#include "RenderStyle.h"

namespace WebCore {
namespace Layout {

InlineLayoutUnit TextUtil::width(const InlineTextItem& inlineTextItem, InlineLayoutUnit contentLogicalLeft)
{
    return TextUtil::width(inlineTextItem, inlineTextItem.start(), inlineTextItem.end(), contentLogicalLeft);
}

InlineLayoutUnit TextUtil::width(const InlineTextItem& inlineTextItem, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft)
{
    RELEASE_ASSERT(from >= inlineTextItem.start());
    RELEASE_ASSERT(to <= inlineTextItem.end());
    if (inlineTextItem.isWhitespace() && !InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem)) {
        auto spaceWidth = inlineTextItem.style().fontCascade().spaceWidth();
        return std::isnan(spaceWidth) ? 0.0f : std::isinf(spaceWidth) ? maxInlineLayoutUnit() : spaceWidth;
    }
    return TextUtil::width(inlineTextItem.inlineTextBox(), from, to, contentLogicalLeft);
}

InlineLayoutUnit TextUtil::width(const InlineTextBox& inlineTextBox, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft)
{
    if (from == to)
        return 0;

    auto& style = inlineTextBox.style();
    auto& font = style.fontCascade();
    auto text = inlineTextBox.content();
    ASSERT(to <= text.length());
    auto hasKerningOrLigatures = font.enableKerning() || font.requiresShaping();
    auto measureWithEndSpace = hasKerningOrLigatures && to < text.length() && text[to] == ' ';
    if (measureWithEndSpace)
        ++to;
    float width = 0;
    if (inlineTextBox.canUseSimplifiedContentMeasuring())
        width = font.widthForSimpleText(StringView(text).substring(from, to - from));
    else {
        auto tabWidth = style.collapseWhiteSpace() ? TabSize(0) : style.tabSize();
        WebCore::TextRun run(StringView(text).substring(from, to - from), contentLogicalLeft);
        if (tabWidth)
            run.setTabSize(true, tabWidth);
        width = font.width(run);
    }

    if (measureWithEndSpace)
        width -= (font.spaceWidth() + font.wordSpacing());

    return std::isnan(width) ? 0.0f : std::isinf(width) ? maxInlineLayoutUnit() : width;
}

TextUtil::MidWordBreak TextUtil::midWordBreak(const InlineTextItem& inlineTextItem, InlineLayoutUnit textWidth, InlineLayoutUnit availableWidth, InlineLayoutUnit contentLogicalLeft)
{
    ASSERT(availableWidth >= 0);
    auto startPosition = inlineTextItem.start();
    auto length = inlineTextItem.length();
    ASSERT(length);

    auto text = inlineTextItem.inlineTextBox().content();
    auto surrogatePairAwareIndex = [&] (auto index) {
        // We should never break in the middle of a surrogate pair. They are considered one joint entity.
        RELEASE_ASSERT(index < text.length());
        if (!U16_IS_LEAD(text[index]))
            return index;
        RELEASE_ASSERT(index + 1 < text.length());
        ASSERT(U16_IS_TRAIL(text[index + 1]));
        return ++index;
    };

    auto left = startPosition;
    // Pathological case of (extremely)long string and narrow lines.
    // Adjust the range so that we can pick a reasonable midpoint.
    auto averageCharacterWidth = InlineLayoutUnit { textWidth / length };
    unsigned offset = toLayoutUnit(2 * availableWidth / averageCharacterWidth).toUnsigned();
    auto right = surrogatePairAwareIndex(std::min<unsigned>(left + offset, (startPosition + length - 1)));
    // Preserve the left width for the final split position so that we don't need to remeasure the left side again.
    auto leftSideWidth = InlineLayoutUnit { 0 };
    while (left < right) {
        auto middle = surrogatePairAwareIndex((left + right) / 2);
        auto width = TextUtil::width(inlineTextItem, startPosition, middle + 1, contentLogicalLeft);
        if (width < availableWidth) {
            left = middle + 1;
            leftSideWidth = width;
        } else if (width > availableWidth) {
            auto surrogatePairAwareStart = [&] (auto index) {
                if (!U16_IS_TRAIL(text[middle]))
                    return index;
                RELEASE_ASSERT(index);
                ASSERT(U16_IS_LEAD(text[index - 1]));
                return --index;
            };
            // When the substring does not fit, the right side is supposed to be the start of the surrogate pair if applicable. 
            right = surrogatePairAwareStart(middle);
        } else {
            right = middle + 1;
            leftSideWidth = width;
            break;
        }
    }
    return { startPosition, right - startPosition, leftSideWidth };
}

unsigned TextUtil::findNextBreakablePosition(LazyLineBreakIterator& lineBreakIterator, unsigned startPosition, const RenderStyle& style)
{
    auto keepAllWordsForCJK = style.wordBreak() == WordBreak::KeepAll;
    auto breakNBSP = style.autoWrap() && style.nbspMode() == NBSPMode::Space;

    if (keepAllWordsForCJK) {
        if (breakNBSP)
            return nextBreakablePositionKeepingAllWords(lineBreakIterator, startPosition);
        return nextBreakablePositionKeepingAllWordsIgnoringNBSP(lineBreakIterator, startPosition);
    }

    if (lineBreakIterator.mode() == LineBreakIteratorMode::Default) {
        if (breakNBSP)
            return WebCore::nextBreakablePosition(lineBreakIterator, startPosition);
        return nextBreakablePositionIgnoringNBSP(lineBreakIterator, startPosition);
    }

    if (breakNBSP)
        return nextBreakablePositionWithoutShortcut(lineBreakIterator, startPosition);
    return nextBreakablePositionIgnoringNBSPWithoutShortcut(lineBreakIterator, startPosition);
}

bool TextUtil::shouldPreserveSpacesAndTabs(const Box& layoutBox)
{
    // https://www.w3.org/TR/css-text-3/#white-space-property
    auto whitespace = layoutBox.style().whiteSpace();
    return whitespace == WhiteSpace::Pre || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::BreakSpaces;
}

bool TextUtil::shouldPreserveNewline(const Box& layoutBox)
{
    auto whitespace = layoutBox.style().whiteSpace();
    // https://www.w3.org/TR/css-text-3/#white-space-property
    return whitespace == WhiteSpace::Pre || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::BreakSpaces || whitespace == WhiteSpace::PreLine; 
}

LineBreakIteratorMode TextUtil::lineBreakIteratorMode(LineBreak lineBreak)
{
    switch (lineBreak) {
    case LineBreak::Auto:
    case LineBreak::AfterWhiteSpace:
    case LineBreak::Anywhere:
        return LineBreakIteratorMode::Default;
    case LineBreak::Loose:
        return LineBreakIteratorMode::Loose;
    case LineBreak::Normal:
        return LineBreakIteratorMode::Normal;
    case LineBreak::Strict:
        return LineBreakIteratorMode::Strict;
    }
    ASSERT_NOT_REACHED();
    return LineBreakIteratorMode::Default;
}

}
}
#endif
