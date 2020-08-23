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

#include "DisplayBox.h"
#include "DisplayInlineRect.h"

namespace WebCore {
namespace Layout {

class LineBox {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct Baseline {
        Baseline(InlineLayoutUnit ascent, InlineLayoutUnit descent);
        Baseline() = default;

        void setAscent(InlineLayoutUnit);
        void setDescent(InlineLayoutUnit);

        void reset();

        InlineLayoutUnit height() const { return ascent() + descent(); }
        InlineLayoutUnit ascent() const;
        InlineLayoutUnit descent() const;

    private:
#if ASSERT_ENABLED
        bool m_hasValidAscent { false };
        bool m_hasValidDescent { false };
#endif
        InlineLayoutUnit m_ascent { 0 };
        InlineLayoutUnit m_descent { 0 };
    };

    LineBox(const Display::InlineRect&, const Baseline&, InlineLayoutUnit baselineOffset);
    LineBox() = default;

    const Display::InlineRect& logicalRect() const { return m_rect; }
    const Display::InlineRect& scrollableOverflow() const { return m_scrollableOverflow; }

    InlineLayoutUnit logicalLeft() const { return m_rect.left(); }
    InlineLayoutUnit logicalRight() const { return m_rect.right(); }
    InlineLayoutUnit logicalTop() const { return m_rect.top(); }
    InlineLayoutUnit logicalBottom() const { return m_rect.bottom(); }

    InlineLayoutUnit logicalWidth() const { return m_rect.width(); }
    InlineLayoutUnit logicalHeight() const { return m_rect.height(); }

    const Baseline& baseline() const;
    // Baseline offset from line logical top. Note that offset does not necessarily equal to ascent.
    //
    // -------------------    line logical top     ------------------- (top align)
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
    // -------------------    line logical bottom  -------------------
    InlineLayoutUnit baselineOffset() const;
    void setBaselineOffsetIfGreater(InlineLayoutUnit);
    void setAscentIfGreater(InlineLayoutUnit);
    void setDescentIfGreater(InlineLayoutUnit);

    void resetBaseline();
    void resetDescent() { m_baseline.setDescent(0_lu); }

    void setLogicalHeight(InlineLayoutUnit logicalHeight) { m_rect.setHeight(logicalHeight); }

    void setLogicalHeightIfGreater(InlineLayoutUnit);
    void setLogicalWidth(InlineLayoutUnit logicalWidth) { m_rect.setWidth(logicalWidth); }

    void setScrollableOverflow(const Display::InlineRect& rect) { m_scrollableOverflow = rect; }

    void moveHorizontally(InlineLayoutUnit delta) { m_rect.moveHorizontally(delta); }

    void expandHorizontally(InlineLayoutUnit delta) { m_rect.expandHorizontally(delta); }
    void shrinkHorizontally(InlineLayoutUnit delta) { expandHorizontally(-delta); }

    void expandVertically(InlineLayoutUnit delta) { m_rect.expandVertically(delta); }
    void shrinkVertically(InlineLayoutUnit delta) { expandVertically(-delta); }

    // https://www.w3.org/TR/CSS22/visuren.html#inline-formatting
    // Line boxes that contain no text, no preserved white space, no inline elements with non-zero margins, padding, or borders,
    // and no other in-flow content (such as images, inline blocks or inline tables), and do not end with a preserved newline
    // must be treated as zero-height line boxes for the purposes of determining the positions of any elements inside of them,
    // and must be treated as not existing for any other purpose.
    // Note that it does not necessarily mean visually non-empty line. <span style="font-size: 0px">this is still considered non-empty</span>
    bool isConsideredEmpty() const { return m_isConsideredEmpty; }
    void setIsConsideredEmpty() { m_isConsideredEmpty = true; }
    void setIsConsideredNonEmpty() { m_isConsideredEmpty = false; }

private:
#if ASSERT_ENABLED
    bool m_hasValidBaseline { false };
    bool m_hasValidBaselineOffset { false };
#endif
    Display::InlineRect m_rect;
    Display::InlineRect m_scrollableOverflow;
    Baseline m_baseline;
    InlineLayoutUnit m_baselineOffset { 0 };
    bool m_isConsideredEmpty { true };
};

inline LineBox::LineBox(const Display::InlineRect& rect, const Baseline& baseline, InlineLayoutUnit baselineOffset)
    : m_rect(rect)
    , m_baseline(baseline)
    , m_baselineOffset(baselineOffset)
{
#if ASSERT_ENABLED
    m_hasValidBaseline = true;
    m_hasValidBaselineOffset = true;
#endif
}

inline void LineBox::setLogicalHeightIfGreater(InlineLayoutUnit logicalHeight)
{
    if (logicalHeight <= m_rect.height())
        return;
    m_rect.setHeight(logicalHeight);
}

inline const LineBox::Baseline& LineBox::baseline() const
{
    ASSERT(m_hasValidBaseline);
    return m_baseline;
}

inline void LineBox::setBaselineOffsetIfGreater(InlineLayoutUnit baselineOffset)
{
#if ASSERT_ENABLED
    m_hasValidBaselineOffset = true;
#endif
    m_baselineOffset = std::max(baselineOffset, m_baselineOffset);
}

inline void LineBox::setAscentIfGreater(InlineLayoutUnit ascent)
{
    if (ascent < m_baseline.ascent())
        return;
    setBaselineOffsetIfGreater(ascent);
    m_baseline.setAscent(ascent);
}

inline void LineBox::setDescentIfGreater(InlineLayoutUnit descent)
{
    if (descent < m_baseline.descent())
        return;
    m_baseline.setDescent(descent);
}

inline InlineLayoutUnit LineBox::baselineOffset() const
{
    ASSERT(m_hasValidBaselineOffset);
    return m_baselineOffset;
}

inline void LineBox::resetBaseline()
{
#if ASSERT_ENABLED
    m_hasValidBaselineOffset = true;
#endif
    m_baselineOffset = 0_lu;
    m_baseline.reset();
}

inline LineBox::Baseline::Baseline(InlineLayoutUnit ascent, InlineLayoutUnit descent)
    : m_ascent(ascent)
    , m_descent(descent)
{
#if ASSERT_ENABLED
    m_hasValidAscent = true;
    m_hasValidDescent = true;
#endif
}

inline void LineBox::Baseline::setAscent(InlineLayoutUnit ascent)
{
#if ASSERT_ENABLED
    m_hasValidAscent = true;
#endif
    m_ascent = ascent;
}

inline void LineBox::Baseline::setDescent(InlineLayoutUnit descent)
{
#if ASSERT_ENABLED
    m_hasValidDescent = true;
#endif
    m_descent = descent;
}

inline void LineBox::Baseline::reset()
{
#if ASSERT_ENABLED
    m_hasValidAscent = true;
    m_hasValidDescent = true;
#endif
    m_ascent = 0_lu;
    m_descent = 0_lu;
}

inline InlineLayoutUnit LineBox::Baseline::ascent() const
{
    ASSERT(m_hasValidAscent);
    return m_ascent;
}

inline InlineLayoutUnit LineBox::Baseline::descent() const
{
    ASSERT(m_hasValidDescent);
    return m_descent;
}

}
}

#endif
