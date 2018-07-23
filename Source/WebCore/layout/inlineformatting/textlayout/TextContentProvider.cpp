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
#include "TextContentProvider.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "Hyphenation.h"
#include "RenderStyle.h"
#include "SimpleTextRunGenerator.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextContentProvider);

TextContentProvider::TextItem::Style::Style(const RenderStyle& style)
    : font(style.fontCascade())
    , collapseWhitespace(style.collapseWhiteSpace())
    , hasKerningOrLigatures(font.enableKerning() || font.requiresShaping())
    , wordSpacing(font.wordSpacing())
    , tabWidth(collapseWhitespace ? 0 : style.tabSize())
    , preserveNewline(style.preserveNewline())
    , breakNBSP(style.autoWrap() && style.nbspMode() == NBSPMode::Space)
    , keepAllWordsForCJK(style.wordBreak() == WordBreak::KeepAll)
    , locale(style.locale())
{
}

TextContentProvider::TextContentProvider()
    : m_textContentIterator(m_textContent)
{
}

TextContentProvider::~TextContentProvider()
{
}

void TextContentProvider::appendText(String text, const RenderStyle& style, bool canUseSimplifiedMeasure)
{
    ASSERT(text.length());

    auto start = length();
    auto end = start + text.length();
    m_textContent.append({ text, start, end, TextItem::Style(style), canUseSimplifiedMeasure });
}

void TextContentProvider::appendLineBreak()
{
    m_hardLineBreaks.append(length());
}

static bool contains(ContentPosition position, const TextContentProvider::TextItem& textItem)
{
    return textItem.start <= position && position < textItem.end;
}

const TextContentProvider::TextItem* TextContentProvider::findTextItemSlow(ContentPosition position) const
{
    // Since this is an iterator like class, check the next item first (instead of starting the search from the beginning).
    if (auto* textItem = (++m_textContentIterator).current()) {
        if (contains(position, *textItem))
            return textItem;
    }

    m_textContentIterator.reset();
    while (auto* textItem = m_textContentIterator.current()) {
        if (contains(position, *textItem))
            return textItem;
        ++m_textContentIterator;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

float TextContentProvider::width(ContentPosition from, ContentPosition to, float xPosition) const
{
    if (from >= to) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    auto contentLength = length();
    if (from >= contentLength || to > contentLength) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    float width = 0;
    auto length = to - from;
    auto* textItem = m_textContentIterator.current();
    auto startPosition = from - (textItem && contains(from, *textItem) ? textItem->start : findTextItemSlow(from)->start);

    while (length) {
        textItem = m_textContentIterator.current();
        if (!textItem) {
            ASSERT_NOT_REACHED();
            break;
        }

        auto endPosition = std::min<ItemPosition>(startPosition + length, textItem->text.length());
        ASSERT(endPosition >= startPosition);
        auto textWidth = this->textWidth(*textItem, startPosition, endPosition, xPosition);

        xPosition += textWidth;
        width += textWidth;
        length -= (endPosition - startPosition);

        startPosition = 0;
        if (length)
            ++m_textContentIterator;
    }

    return width;
}

float TextContentProvider::textWidth(const TextItem& textItem, ItemPosition from, ItemPosition to, float xPosition) const
{
    if (from > to || to > textItem.end - textItem.start) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    auto& style = textItem.style;
    auto& font = style.font;
    if (!font.size() || from == to)
        return 0;

    if (font.isFixedPitch())
        return fixedPitchWidth(textItem.text, style, from, to, xPosition);

    auto measureWithEndSpace = style.hasKerningOrLigatures && to < textItem.text.length() && textItem.text[to] == ' ';
    if (measureWithEndSpace)
        ++to;
    float width = 0;
    if (textItem.canUseSimplifiedMeasure)
        width = font.widthForSimpleText(StringView(textItem.text).substring(from, to - from));
    else  {
        WebCore::TextRun run(StringView(textItem.text).substring(from, to - from), xPosition);
        if (style.tabWidth)
            run.setTabSize(true, style.tabWidth);
        width = font.width(run);
    }

    if (measureWithEndSpace)
        width -= (font.spaceWidth() + style.wordSpacing);

    return std::max<float>(0, width);
}

float TextContentProvider::fixedPitchWidth(String text, const TextItem::Style& style, ItemPosition from, ItemPosition to, float xPosition) const
{
    auto monospaceCharacterWidth = style.font.spaceWidth();
    float width = 0;
    for (auto i = from; i < to; ++i) {
        auto character = text[i];
        if (character >= ' ' || character == '\n')
            width += monospaceCharacterWidth;
        else if (character == '\t')
            width += style.collapseWhitespace ? monospaceCharacterWidth : style.font.tabWidth(style.tabWidth, xPosition + width);

        if (i > from && (character == ' ' || character == '\t' || character == '\n'))
            width += style.font.wordSpacing();
    }

    return width;
}

std::optional<ContentPosition> TextContentProvider::hyphenPositionBefore(ContentPosition from, ContentPosition to, ContentPosition before) const
{
    auto contentLength = length();
    if (before >= contentLength || before < from || before > to) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto* textItem = m_textContentIterator.current();
    if (!textItem || !contains(before, *textItem))
        textItem = findTextItemSlow(before);

    auto fromItemPosition = from - textItem->start;
    auto stringForHyphenLocation = StringView(textItem->text).substring(fromItemPosition, to - from);

    // adjustedBefore -> ContentPosition -> ItemPosition -> run position.
    auto adjustedBefore = before - from;
    auto hyphenLocation = lastHyphenLocation(stringForHyphenLocation, adjustedBefore, textItem->style.locale);
    if (!hyphenLocation)
        return { };

    return from + hyphenLocation;
}

unsigned TextContentProvider::length() const
{
    if (!m_textContent.size())
        return 0;
    return m_textContent.last().end;
}

TextContentProvider::Iterator TextContentProvider::iterator()
{
    // TODO: This is where we decide whether we need a simple or a more complex provider.
    if (!m_simpleTextRunGenerator)
        m_simpleTextRunGenerator = std::make_unique<SimpleTextRunGenerator>(*this);
    else
        m_simpleTextRunGenerator->reset();

    m_simpleTextRunGenerator->findNextRun();
    return Iterator(*this);
}

TextContentProvider::TextRunList TextContentProvider::textRuns()
{
    TextRunList textRunList;

    auto textRunIterator = iterator();
    while (auto textRum = textRunIterator.current()) {
        textRunList.append(*textRum);
        ++textRunIterator;
    }
    return textRunList;
}

void TextContentProvider::findNextRun()
{
    m_simpleTextRunGenerator->findNextRun();
}

std::optional<TextRun> TextContentProvider::current() const
{
    return m_simpleTextRunGenerator->current();
}

}
}
#endif
