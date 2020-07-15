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

namespace WebCore {
namespace Display {

class LineBox {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct Baseline {
        Baseline(InlineLayoutUnit ascent, InlineLayoutUnit descent);

        InlineLayoutUnit height() const { return ascent() + descent(); }
        InlineLayoutUnit ascent() const { return m_ascent; }
        InlineLayoutUnit descent() const { return m_descent; }

    private:
        InlineLayoutUnit m_ascent { 0 };
        InlineLayoutUnit m_descent { 0 };
    };

    LineBox(const InlineRect&, const InlineRect& scrollableOverflow, const InlineRect& inkOverflow, const Baseline&, InlineLayoutUnit baselineOffset);

    const InlineRect& rect() const { return m_rect; }
    const InlineRect& scrollableOverflow() const { return m_scrollableOverflow; }
    const InlineRect& inkOverflow() const { return m_inkOverflow; }

    InlineLayoutUnit left() const { return m_rect.left(); }
    InlineLayoutUnit right() const { return m_rect.right(); }
    InlineLayoutUnit top() const { return m_rect.top(); }
    InlineLayoutUnit bottom() const { return m_rect.bottom(); }

    InlineLayoutUnit width() const { return m_rect.width(); }
    InlineLayoutUnit height() const { return m_rect.height(); }

    void moveVertically(InlineLayoutUnit);

    const Baseline& baseline() const { return m_baseline; }
    // Baseline offset from line top. Note that offset does not necessarily equal to ascent.
    //
    // -------------------        line top         ------------------- (top align)
    //             ^                                              ^
    //             |                                  ^           |
    //   ^         | baseline offset                  |           | baseline offset
    //   |         |                                  |           |
    //   | ascent  |                                  | ascent    |
    //   |         |                                  v           v
    //   v         v                               ------------------- baseline
    //   ----------------- baseline                   ^
    //   ^                                            | descent
    //   | descent                                    v
    //   v
    // -------------------       line bottom       -------------------
    InlineLayoutUnit baselineOffset() const { return m_baselineOffset; }

private:
    InlineRect m_rect;
    InlineRect m_scrollableOverflow;
    InlineRect m_inkOverflow;
    Baseline m_baseline;
    InlineLayoutUnit m_baselineOffset { 0 };
};

inline LineBox::LineBox(const InlineRect& rect, const InlineRect& scrollableOverflow, const InlineRect& inkOverflow, const Baseline& baseline, InlineLayoutUnit baselineOffset)
    : m_rect(rect)
    , m_scrollableOverflow(scrollableOverflow)
    , m_inkOverflow(inkOverflow)
    , m_baseline(baseline)
    , m_baselineOffset(baselineOffset)
{
}

inline LineBox::Baseline::Baseline(InlineLayoutUnit ascent, InlineLayoutUnit descent)
    : m_ascent(ascent)
    , m_descent(descent)
{
}

inline void LineBox::moveVertically(InlineLayoutUnit offset)
{
    m_rect.moveVertically(offset);
    m_inkOverflow.moveVertically(offset);
}

}
}

#endif
