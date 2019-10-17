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

#include "DisplayRect.h"
#include "LayoutUnit.h"
#include "RenderStyle.h"

namespace WebCore {

class CachedImage;

namespace Display {

struct Run {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    struct TextContext {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
    public:
        TextContext(unsigned position, unsigned length, String content);

        unsigned start() const { return m_start; }
        unsigned end() const { return start() + length(); }
        unsigned length() const { return m_length; }
        String content() const { return m_content; }

        void expand(const TextContext& other);

    private:
        unsigned m_start;
        unsigned m_length;
        // FIXME: This is temporary. We should have some mapping setup to identify associated text content instead.
        String m_content;
    };

    Run(const RenderStyle&, Rect logicalRect, Optional<TextContext> = WTF::nullopt);

    const Rect& logicalRect() const { return m_logicalRect; }

    LayoutPoint logicalTopLeft() const { return m_logicalRect.topLeft(); }
    LayoutUnit logicalLeft() const { return m_logicalRect.left(); }
    LayoutUnit logicalRight() const { return m_logicalRect.right(); }
    LayoutUnit logicalTop() const { return m_logicalRect.top(); }
    LayoutUnit logicalBottom() const { return m_logicalRect.bottom(); }

    LayoutUnit logicalWidth() const { return m_logicalRect.width(); }
    LayoutUnit logicalHeight() const { return m_logicalRect.height(); }

    void setLogicalWidth(LayoutUnit width) { m_logicalRect.setWidth(width); }
    void setLogicalTop(LayoutUnit logicalTop) { m_logicalRect.setTop(logicalTop); }
    void setLogicalLeft(LayoutUnit logicalLeft) { m_logicalRect.setLeft(logicalLeft); }
    void setLogicalRight(LayoutUnit logicalRight) { m_logicalRect.shiftRightTo(logicalRight); }
    void moveVertically(LayoutUnit delta) { m_logicalRect.moveVertically(delta); }
    void moveHorizontally(LayoutUnit delta) { m_logicalRect.moveHorizontally(delta); }
    void expandVertically(LayoutUnit delta) { m_logicalRect.expandVertically(delta); }
    void expandHorizontally(LayoutUnit delta) { m_logicalRect.expandHorizontally(delta); }

    void setTextContext(TextContext textContext) { m_textContext.emplace(textContext); }
    Optional<TextContext>& textContext() { return m_textContext; }
    Optional<TextContext> textContext() const { return m_textContext; }

    void setImage(CachedImage& image) { m_cachedImage = &image; }
    CachedImage* image() const { return m_cachedImage; }

    const RenderStyle& style() const { return m_style; }

private:
    // FIXME: Find out the Display::Run <-> paint style setup.
    const RenderStyle& m_style;
    CachedImage* m_cachedImage { nullptr };
    Rect m_logicalRect;
    Optional<TextContext> m_textContext;
};

inline Run::Run(const RenderStyle& style, Rect logicalRect, Optional<TextContext> textContext)
    : m_style(style)
    , m_logicalRect(logicalRect)
    , m_textContext(textContext)
{
}

inline Run::TextContext::TextContext(unsigned start, unsigned length, String content)
    : m_start(start)
    , m_length(length)
    , m_content(content)
{
}

inline void Run::TextContext::expand(const TextContext& other)
{
    m_content.append(other.content());
    m_length += other.length();
}

}
}
#endif
