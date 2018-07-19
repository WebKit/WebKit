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

#include "TextContentProvider.h"
#include <wtf/IsoMalloc.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

class RenderStyle;

namespace Layout {

class SimpleTextRunGenerator {
    WTF_MAKE_ISO_ALLOCATED(SimpleTextRunGenerator);
public:
    SimpleTextRunGenerator(const TextContentProvider&);

    void findNextRun();
    std::optional<TextRun> current() const;
    void reset();

private:
    void moveToNextBreakablePosition();
    bool moveToNextNonWhitespacePosition();
    bool isAtLineBreak() const;
    bool isAtSoftLineBreak() const;
    ItemPosition toItemPosition(ContentPosition) const;

    struct Position {
        Position() = default;

        bool operator==(ContentPosition position) const { return m_contentPosition == position; }
        bool operator<(ContentPosition position) const { return m_contentPosition < position; }
        operator ContentPosition() const { return contentPosition(); }
        Position& operator++();
        Position& operator+=(unsigned);

        void resetItemPosition() { m_itemPosition = 0; }

        ContentPosition contentPosition() const { return m_contentPosition; }
        ItemPosition itemPosition() const { return m_itemPosition; }

    private:
        ContentPosition m_contentPosition { 0 };
        ItemPosition m_itemPosition { 0 };
    };

    const TextContentProvider& m_contentProvider;
    ConstVectorIterator<TextContentProvider::TextItem> m_textIterator;
    ConstVectorIterator<ContentPosition> m_hardLineBreakIterator;

    Position m_currentPosition;
    std::optional<TextRun> m_currentRun;
    float m_xPosition { 0 };

    LazyLineBreakIterator m_lineBreakIterator;
};

inline SimpleTextRunGenerator::Position& SimpleTextRunGenerator::Position::operator++()
{
    ++m_contentPosition;
    ++m_itemPosition;
    return *this;
}

inline SimpleTextRunGenerator::Position& SimpleTextRunGenerator::Position::operator+=(unsigned advance)
{
    m_contentPosition += advance;
    m_itemPosition += advance;
    return *this;
}

}
}
#endif
