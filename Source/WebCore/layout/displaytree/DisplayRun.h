/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "DisplayInlineRect.h"
#include "LayoutBox.h"
#include "RenderStyle.h"
#include "TextFlags.h"

namespace WebCore {

class CachedImage;

namespace Display {

struct Run {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    struct TextContext {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
    public:
        struct ExpansionContext;
        TextContext(unsigned position, unsigned length, const String&);

        unsigned start() const { return m_start; }
        unsigned end() const { return start() + length(); }
        unsigned length() const { return m_length; }
        StringView content() const { return StringView(m_contentString).substring(m_start, m_length); }

        struct ExpansionContext {
            ExpansionBehavior behavior;
            InlineLayoutUnit horizontalExpansion { 0 };
        };
        void setExpansion(ExpansionContext expansionContext) { m_expansionContext = expansionContext; }
        Optional<ExpansionContext> expansion() const { return m_expansionContext; }

        bool needsHyphen() const { return m_needsHyphen; }

        void expand(unsigned expandedLength);
        void setNeedsHyphen() { m_needsHyphen = true; }

    private:
        unsigned m_start { 0 };
        unsigned m_length { 0 };
        bool m_needsHyphen { false };
        String m_contentString;
        Optional<ExpansionContext> m_expansionContext;
    };

    Run(size_t lineIndex, const Layout::Box&, const InlineRect& logicalRect, const InlineRect& inkOverflow, Optional<TextContext> = WTF::nullopt);

    size_t lineIndex() const { return m_lineIndex; }

    const InlineRect& logicalRect() const { return m_logicalRect; }
    const InlineRect& inkOverflow() const { return m_inkOverflow; }

    InlineLayoutPoint logicalTopLeft() const { return m_logicalRect.topLeft(); }
    InlineLayoutUnit logicalLeft() const { return m_logicalRect.left(); }
    InlineLayoutUnit logicalRight() const { return m_logicalRect.right(); }
    InlineLayoutUnit logicalTop() const { return m_logicalRect.top(); }
    InlineLayoutUnit logicalBottom() const { return m_logicalRect.bottom(); }

    InlineLayoutUnit logicalWidth() const { return m_logicalRect.width(); }
    InlineLayoutUnit logicalHeight() const { return m_logicalRect.height(); }

    void setLogicalWidth(InlineLayoutUnit width) { m_logicalRect.setWidth(width); }
    void setLogicalTop(InlineLayoutUnit logicalTop) { m_logicalRect.setTop(logicalTop); }
    void setLogicalLeft(InlineLayoutUnit logicalLeft) { m_logicalRect.setLeft(logicalLeft); }
    void moveVertically(InlineLayoutUnit delta) { m_logicalRect.moveVertically(delta); }
    void moveHorizontally(InlineLayoutUnit delta) { m_logicalRect.moveHorizontally(delta); }
    void expandVertically(InlineLayoutUnit delta) { m_logicalRect.expandVertically(delta); }
    void expandHorizontally(InlineLayoutUnit delta) { m_logicalRect.expandHorizontally(delta); }

    void setTextContext(const TextContext&& textContext) { m_textContext.emplace(textContext); }
    const Optional<TextContext>& textContext() const { return m_textContext; }
    Optional<TextContext>& textContext() { return m_textContext; }

    void setImage(CachedImage& image) { m_cachedImage = &image; }
    CachedImage* image() const { return m_cachedImage; }

    bool isLineBreak() const { return layoutBox().isLineBreakBox() || (textContext() && textContext()->content() == "\n" && style().preserveNewline()); }

    const Layout::Box& layoutBox() const { return *m_layoutBox; }
    const RenderStyle& style() const { return m_layoutBox->style(); }

private:
    // FIXME: Find out the Display::Run <-> paint style setup.
    const size_t m_lineIndex;
    WeakPtr<const Layout::Box> m_layoutBox;
    CachedImage* m_cachedImage { nullptr };
    InlineRect m_logicalRect;
    InlineRect m_inkOverflow;
    Optional<TextContext> m_textContext;
};

inline Run::Run(size_t lineIndex, const Layout::Box& layoutBox, const InlineRect& logicalRect, const InlineRect& inkOverflow, Optional<TextContext> textContext)
    : m_lineIndex(lineIndex)
    , m_layoutBox(makeWeakPtr(layoutBox))
    , m_logicalRect(logicalRect)
    , m_inkOverflow(inkOverflow)
    , m_textContext(textContext)
{
}

inline Run::TextContext::TextContext(unsigned start, unsigned length, const String& contentString)
    : m_start(start)
    , m_length(length)
    , m_contentString(contentString)
{
}

inline void Run::TextContext::expand(unsigned expandedLength)
{
    m_length = expandedLength;
}

}
}
#endif
