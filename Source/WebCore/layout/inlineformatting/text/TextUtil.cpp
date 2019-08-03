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

#include "FontCascade.h"
#include "RenderStyle.h"

namespace WebCore {
namespace Layout {

Optional<unsigned> TextUtil::hyphenPositionBefore(const InlineItem&, unsigned, unsigned)
{
    return WTF::nullopt;
}

LayoutUnit TextUtil::width(const Box& inlineBox, unsigned from, unsigned to, LayoutUnit contentLogicalLeft)
{
    auto& style = inlineBox.style();
    auto& font = style.fontCascade();
    if (!font.size() || from == to)
        return 0;

    auto text = inlineBox.textContent();
    ASSERT(to <= text.length());

    if (font.isFixedPitch())
        return fixedPitchWidth(text, style, from, to, contentLogicalLeft);

    auto hasKerningOrLigatures = font.enableKerning() || font.requiresShaping();
    auto measureWithEndSpace = hasKerningOrLigatures && to < text.length() && text[to] == ' ';
    if (measureWithEndSpace)
        ++to;
    LayoutUnit width;
    auto tabWidth = style.collapseWhiteSpace() ? TabSize(0) : style.tabSize();

    WebCore::TextRun run(StringView(text).substring(from, to - from), contentLogicalLeft);
    if (tabWidth)
        run.setTabSize(true, tabWidth);
    width = font.width(run);

    if (measureWithEndSpace)
        width -= (font.spaceWidth() + font.wordSpacing());

    return std::max<LayoutUnit>(0, width);
}

LayoutUnit TextUtil::fixedPitchWidth(String text, const RenderStyle& style, unsigned from, unsigned to, LayoutUnit contentLogicalLeft)
{
    auto& font = style.fontCascade();
    auto monospaceCharacterWidth = font.spaceWidth();
    LayoutUnit width;
    for (auto i = from; i < to; ++i) {
        auto character = text[i];
        if (character >= ' ' || character == '\n')
            width += monospaceCharacterWidth;
        else if (character == '\t')
            width += style.collapseWhiteSpace() ? monospaceCharacterWidth : font.tabWidth(style.tabSize(), contentLogicalLeft + width);

        if (i > from && (character == ' ' || character == '\t' || character == '\n'))
            width += font.wordSpacing();
    }

    return width;
}

bool TextUtil::isTrimmableContent(const InlineItem& inlineItem)
{
    if (!is<InlineTextItem>(inlineItem))
        return false;
    auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
    return inlineTextItem.isWhitespace() && inlineTextItem.style().collapseWhiteSpace();
}

TextUtil::SplitData TextUtil::split(const Box& inlineBox, unsigned startPosition, unsigned length, LayoutUnit textWidth, LayoutUnit availableWidth, LayoutUnit contentLogicalLeft)
{
    // FIXME This should take hypens into account.
    ASSERT(availableWidth >= 0);
    auto left = startPosition;
    // Pathological case of (extremely)long string and narrow lines.
    // Adjust the range so that we can pick a reasonable midpoint.
    LayoutUnit averageCharacterWidth = textWidth / length;
    auto right = std::min<unsigned>(left + (2 * availableWidth / averageCharacterWidth).toUnsigned(), (startPosition + length - 1));
    // Preserve the left width for the final split position so that we don't need to remeasure the left side again.
    LayoutUnit leftSideWidth = 0;
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

}
}
#endif
