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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "Runs.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class FontCascade;
class RenderStyle;

namespace Layout {

class SimpleTextRunGenerator;

class TextContentProvider {
    WTF_MAKE_ISO_ALLOCATED(TextContentProvider);
public:
    TextContentProvider();
    ~TextContentProvider();

    void appendText(String, const RenderStyle&, bool canUseSimplifiedMeasure);
    void appendLineBreak();

    struct TextItem {
        struct Style {
            explicit Style(const RenderStyle&);

            const FontCascade& font;
            bool collapseWhitespace { false };
            bool hasKerningOrLigatures { false };
            float wordSpacing { 0 };
            float tabWidth { 0 };
            bool preserveNewline { false };
            bool breakNBSP { false };
            bool keepAllWordsForCJK { false };
            AtomicString locale;
        };

        const String text;
        const ContentPosition start { 0 };
        const ContentPosition end { 0 };
        const Style style;
        const bool canUseSimplifiedMeasure { false };
    };
    using TextContent = Vector<TextItem>;
    const TextContent& textContent() const { return m_textContent; }

    using HardLineBreaks = Vector<ContentPosition>;
    const HardLineBreaks& hardLineBreaks() const { return m_hardLineBreaks; }

    unsigned length() const;
    float width(ContentPosition from, ContentPosition to, float xPosition) const;
    std::optional<ContentPosition> hyphenPositionBefore(ContentPosition from, ContentPosition to, ContentPosition before) const;

    class Iterator {
    public:
        Iterator& operator++();
        std::optional<TextRun> current() const { return m_contentProvider.current(); };

    private:
        friend class TextContentProvider;
        Iterator(TextContentProvider&);

        TextContentProvider& m_contentProvider;
    };
    Iterator iterator();

    using TextRunList = Vector<TextRun>;
    TextRunList textRuns();

private:
    friend class Iterator;

    const TextItem* findTextItemSlow(ContentPosition) const;
    float textWidth(const TextItem&, ItemPosition from, ItemPosition to, float xPosition) const;
    float fixedPitchWidth(String, const TextItem::Style&, ItemPosition from, ItemPosition to, float xPosition) const;

    void findNextRun();
    std::optional<TextRun> current() const;

    TextContent m_textContent;
    mutable ConstVectorIterator<TextItem> m_textContentIterator;
    HardLineBreaks m_hardLineBreaks;
    std::unique_ptr<SimpleTextRunGenerator> m_simpleTextRunGenerator;
};

inline TextContentProvider::Iterator::Iterator(TextContentProvider& contentProvider)
    : m_contentProvider(contentProvider)
{
}

inline TextContentProvider::Iterator& TextContentProvider::Iterator::operator++()
{
    m_contentProvider.findNextRun();
    return *this;
}

}
}
#endif
