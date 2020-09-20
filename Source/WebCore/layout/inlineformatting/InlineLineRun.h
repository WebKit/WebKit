/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "InlineRect.h"
#include "LayoutBox.h"
#include "TextFlags.h"

namespace WebCore {
namespace Layout {

struct LineRun {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    struct Text {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
    public:
        Text(size_t position, size_t length, const String&, bool needsHyphen);

        size_t start() const { return m_start; }
        size_t end() const { return start() + length(); }
        size_t length() const { return m_length; }
        String content() const { return m_contentString; }

        bool needsHyphen() const { return m_needsHyphen; }
        void setNeedsHyphen() { m_needsHyphen = true; }

        void expand(size_t delta) { m_length += delta; }
        void shrink(size_t delta) { m_length -= delta; }

    private:
        size_t m_start { 0 };
        size_t m_length { 0 };
        bool m_needsHyphen { false };
        String m_contentString;
    };

    struct Expansion;
    LineRun(size_t lineIndex, const Box&, const InlineRect&, Expansion, Optional<Text> = WTF::nullopt);

    const InlineRect& logicalRect() const { return m_logicalRect; }

    InlineLayoutUnit logicalTop() const { return logicalRect().top(); }
    InlineLayoutUnit logicalBottom() const { return logicalRect().bottom(); }
    InlineLayoutUnit logicalLeft() const { return logicalRect().left(); }
    InlineLayoutUnit logicalRight() const { return logicalRect().right(); }

    InlineLayoutUnit logicalWidth() const { return logicalRect().width(); }
    InlineLayoutUnit logicalHeight() const { return logicalRect().height(); }

    void moveVertically(InlineLayoutUnit offset) { m_logicalRect.moveVertically(offset); }
    Optional<Text>& text() { return m_text; }
    const Optional<Text>& text() const { return m_text; }

    struct Expansion {
        ExpansionBehavior behavior { DefaultExpansion };
        InlineLayoutUnit horizontalExpansion { 0 };
    };
    Expansion expansion() const { return m_expansion; }

    const Box& layoutBox() const { return *m_layoutBox; }
    size_t lineIndex() const { return m_lineIndex; }

private:
    const size_t m_lineIndex;
    WeakPtr<const Layout::Box> m_layoutBox;
    InlineRect m_logicalRect;
    Expansion m_expansion;
    Optional<Text> m_text;
};

inline LineRun::LineRun(size_t lineIndex, const Layout::Box& layoutBox, const InlineRect& logicalRect, Expansion expansion, Optional<Text> text)
    : m_lineIndex(lineIndex)
    , m_layoutBox(makeWeakPtr(layoutBox))
    , m_logicalRect(logicalRect)
    , m_expansion(expansion)
    , m_text(text)
{
}

inline LineRun::Text::Text(size_t start, size_t length, const String& contentString, bool needsHyphen)
    : m_start(start)
    , m_length(length)
    , m_needsHyphen(needsHyphen)
    , m_contentString(contentString)
{
}

}
}
#endif
