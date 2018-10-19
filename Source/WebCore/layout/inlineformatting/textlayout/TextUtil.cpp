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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextUtil);

TextUtil::TextUtil(const InlineContent& inlineContent)
    : m_inlineContent(inlineContent)
{
}

LayoutUnit TextUtil::width(const InlineItem& inlineItem, ItemPosition from, unsigned length, LayoutUnit contentLogicalLeft) const
{
    LayoutUnit width;
    auto startPosition = from;
    auto iterator = m_inlineContent.find<const InlineItem&, InlineItemHashTranslator>(inlineItem);
    auto inlineItemEnd = m_inlineContent.end();
    while (length) {
        ASSERT(iterator != inlineItemEnd);
        auto& currentInlineItem = **iterator;
        auto endPosition = std::min<ItemPosition>(startPosition + length, currentInlineItem.textContent().length());
        auto textWidth = this->textWidth(currentInlineItem, startPosition, endPosition, contentLogicalLeft);

        contentLogicalLeft += textWidth;
        width += textWidth;
        length -= (endPosition - startPosition);

        startPosition = 0;
        ++iterator;
    }

    return width;
}

std::optional<ItemPosition> TextUtil::hyphenPositionBefore(const InlineItem&, ItemPosition, unsigned) const
{
    return std::nullopt;
}

LayoutUnit TextUtil::textWidth(const InlineItem& inlineTextItem, ItemPosition from, ItemPosition to, LayoutUnit contentLogicalLeft) const
{
    auto& style = inlineTextItem.style();
    auto& font = style.fontCascade();
    if (!font.size() || from == to)
        return 0;

    auto text = inlineTextItem.textContent();
    if (font.isFixedPitch())
        return fixedPitchWidth(text, style, from, to, contentLogicalLeft);

    auto hasKerningOrLigatures = font.enableKerning() || font.requiresShaping();
    auto measureWithEndSpace = hasKerningOrLigatures && to < text.length() && text[to] == ' ';
    if (measureWithEndSpace)
        ++to;
    LayoutUnit width;
    auto tabWidth = style.collapseWhiteSpace() ? 0 : style.tabSize();

    WebCore::TextRun run(StringView(text).substring(from, to - from), contentLogicalLeft);
    if (tabWidth)
        run.setTabSize(true, tabWidth);
    width = font.width(run);

    if (measureWithEndSpace)
        width -= (font.spaceWidth() + font.wordSpacing());

    return std::max<LayoutUnit>(0, width);
}

LayoutUnit TextUtil::fixedPitchWidth(String text, const RenderStyle& style, ItemPosition from, ItemPosition to, LayoutUnit contentLogicalLeft) const
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

}
}
#endif
