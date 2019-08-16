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
#include "InlineItem.h"
#include "LayoutUnit.h"
#include "TextFlags.h"

namespace WebCore {
namespace Display {

struct Run {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    struct TextContext {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
    public:
        TextContext(unsigned position, unsigned length);

        unsigned start() const { return m_start; }
        unsigned end() const { return start() + length(); }
        unsigned length() const { return m_length; }

        void expand(unsigned length) { m_length += length; }

    private:
        unsigned m_start;
        unsigned m_length;
    };

    Run(Rect logicalRect);
    Run(Rect logicalRect, TextContext);
    Run(const Run&);

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
    void expandVertically(LayoutUnit delta) { m_logicalRect.expand(0, delta); }
    void expandHorizontally(LayoutUnit delta) { m_logicalRect.expand(delta, 0); }

    void setTextContext(TextContext textContext) { m_textContext.emplace(textContext); }
    Optional<TextContext>& textContext() { return m_textContext; }
    Optional<TextContext> textContext() const { return m_textContext; }

private:
    Rect m_logicalRect;
    Optional<TextContext> m_textContext;
};

inline Run::Run(Rect logicalRect)
    : m_logicalRect(logicalRect)
{
}

inline Run::Run(Rect logicalRect, TextContext textContext)
    : m_logicalRect(logicalRect)
    , m_textContext(textContext)
{
}

inline Run::TextContext::TextContext(unsigned start, unsigned length)
    : m_start(start)
    , m_length(length)
{
}

inline Run::Run(const Run& other)
{
    m_logicalRect = other.m_logicalRect;
    m_textContext = other.m_textContext;
}

}
}
#endif
