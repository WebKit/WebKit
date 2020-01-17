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
#include "RenderBox.h"
#include "RenderStyle.h"

namespace WebCore {
namespace Layout {

InlineLayoutUnit TextUtil::width(const InlineTextItem& inlineTextItem, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft)
{
    // Fast path for collapsed whitespace.
    if (inlineTextItem.isCollapsible()) {
        auto font = inlineTextItem.style().fontCascade();
        return font.spaceWidth() + font.wordSpacing();
    }
    return TextUtil::width(inlineTextItem.layoutBox(), from, to, contentLogicalLeft);
}

InlineLayoutUnit TextUtil::width(const Box& inlineBox, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft)
{
    auto& style = inlineBox.style();
    auto& font = style.fontCascade();
    if (!font.size() || from == to)
        return 0;

    auto& textContext = *inlineBox.textContext();
    auto& text = textContext.content;
    ASSERT(to <= text.length());
    auto hasKerningOrLigatures = font.enableKerning() || font.requiresShaping();
    auto measureWithEndSpace = hasKerningOrLigatures && to < text.length() && text[to] == ' ';
    if (measureWithEndSpace)
        ++to;
    float width = 0;
    if (textContext.canUseSimplifiedContentMeasuring) {
        if (font.isFixedPitch())
            width = fixedPitchWidth(text, style, from, to, contentLogicalLeft);
        else
            width = font.widthForSimpleText(StringView(text).substring(from, to - from));
    } else {
        auto tabWidth = style.collapseWhiteSpace() ? TabSize(0) : style.tabSize();
        WebCore::TextRun run(StringView(text).substring(from, to - from), contentLogicalLeft);
        if (tabWidth)
            run.setTabSize(true, tabWidth);
        width = font.width(run);
    }

    if (measureWithEndSpace)
        width -= (font.spaceWidth() + font.wordSpacing());

    return std::max<InlineLayoutUnit>(0 , InlineLayoutUnit(width));
}

InlineLayoutUnit TextUtil::fixedPitchWidth(const StringView& text, const RenderStyle& style, unsigned from, unsigned to, InlineLayoutUnit contentLogicalLeft)
{
    auto& font = style.fontCascade();
    auto monospaceCharacterWidth = font.spaceWidth();
    float width = 0;
    for (auto i = from; i < to; ++i) {
        auto character = text[i];
        if (character >= ' ' || character == '\n')
            width += monospaceCharacterWidth;
        else if (character == '\t')
            width += style.collapseWhiteSpace() ? monospaceCharacterWidth : font.tabWidth(style.tabSize(), contentLogicalLeft + width);

        if (i > from && (character == ' ' || character == '\t' || character == '\n'))
            width += font.wordSpacing();
    }

    return std::max<InlineLayoutUnit>(0, InlineLayoutUnit(width));
}

TextUtil::SplitData TextUtil::split(const Box& inlineBox, unsigned startPosition, unsigned length, InlineLayoutUnit textWidth, InlineLayoutUnit availableWidth, InlineLayoutUnit contentLogicalLeft)
{
    ASSERT(availableWidth >= 0);
    auto left = startPosition;
    // Pathological case of (extremely)long string and narrow lines.
    // Adjust the range so that we can pick a reasonable midpoint.
    InlineLayoutUnit averageCharacterWidth = textWidth / length;
    unsigned offset = toLayoutUnit(2 * availableWidth / averageCharacterWidth).toUnsigned();
    auto right = std::min<unsigned>(left + offset, (startPosition + length - 1));
    // Preserve the left width for the final split position so that we don't need to remeasure the left side again.
    InlineLayoutUnit leftSideWidth = 0;
    while (left < right) {
        auto middle = (left + right) / 2;
        auto width = TextUtil::width(inlineBox, startPosition, middle + 1, contentLogicalLeft);
        if (width < availableWidth) {
            left = middle + 1;
            leftSideWidth = width;
        } else if (width > availableWidth)
            right = middle;
        else {
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

}
}
#endif
