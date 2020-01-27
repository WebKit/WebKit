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
            ExpansionBehavior behavior { DefaultExpansion };
            InlineLayoutUnit horizontalExpansion { 0 };
        };
        void setExpansion(ExpansionContext expansionContext) { m_expansionContext = expansionContext; }
        Optional<ExpansionContext> expansion() const { return m_expansionContext; }

        bool needsHyphen() const { return m_needsHyphen; }

        void expand(unsigned delta) { m_length += delta; }
        void setNeedsHyphen() { m_needsHyphen = true; }

    private:
        unsigned m_start { 0 };
        unsigned m_length { 0 };
        bool m_needsHyphen { false };
        String m_contentString;
        Optional<ExpansionContext> m_expansionContext;
    };

    Run(size_t lineIndex, const Layout::Box&, const InlineRect&, const InlineRect& inkOverflow, Optional<TextContext> = WTF::nullopt);

    size_t lineIndex() const { return m_lineIndex; }

    const InlineRect& rect() const { return m_rect; }
    const InlineRect& inkOverflow() const { return m_inkOverflow; }

    InlineLayoutPoint topLeft() const { return m_rect.topLeft(); }
    InlineLayoutUnit left() const { return m_rect.left(); }
    InlineLayoutUnit right() const { return m_rect.right(); }
    InlineLayoutUnit top() const { return m_rect.top(); }
    InlineLayoutUnit bottom() const { return m_rect.bottom(); }

    InlineLayoutUnit width() const { return m_rect.width(); }
    InlineLayoutUnit height() const { return m_rect.height(); }

    void setWidth(InlineLayoutUnit width) { m_rect.setWidth(width); }
    void setTop(InlineLayoutUnit top) { m_rect.setTop(top); }
    void setlLeft(InlineLayoutUnit left) { m_rect.setLeft(left); }
    void moveVertically(InlineLayoutUnit delta) { m_rect.moveVertically(delta); }
    void moveHorizontally(InlineLayoutUnit delta) { m_rect.moveHorizontally(delta); }
    void expandVertically(InlineLayoutUnit delta) { m_rect.expandVertically(delta); }
    void expandHorizontally(InlineLayoutUnit delta) { m_rect.expandHorizontally(delta); }

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
    InlineRect m_rect;
    InlineRect m_inkOverflow;
    Optional<TextContext> m_textContext;
};

inline Run::Run(size_t lineIndex, const Layout::Box& layoutBox, const InlineRect& rect, const InlineRect& inkOverflow, Optional<TextContext> textContext)
    : m_lineIndex(lineIndex)
    , m_layoutBox(makeWeakPtr(layoutBox))
    , m_rect(rect)
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

}
}
#endif
