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

#include "DisplayBox.h"

namespace WebCore {
namespace Layout {

class LineBox {
public:
    struct Baseline {
        Baseline(LayoutUnit ascent, LayoutUnit descent);
        Baseline() = default;

        void setAscent(LayoutUnit);
        void setDescent(LayoutUnit);
        void setAscentIfGreater(LayoutUnit);
        void setDescentIfGreater(LayoutUnit);

        void reset();

        LayoutUnit height() const { return ascent() + descent(); }
        LayoutUnit ascent() const;
        LayoutUnit descent() const;

    private:
#if !ASSERT_DISABLED
        bool m_hasValidAscent { false };
        bool m_hasValidDescent { false };
#endif
        LayoutUnit m_ascent;
        LayoutUnit m_descent;
    };

    LineBox(Display::Rect, const Baseline&, LayoutUnit baselineOffset);
    LineBox() = default;
    
    LayoutPoint logicalTopLeft() const { return m_rect.topLeft(); }

    LayoutUnit logicalLeft() const { return m_rect.left(); }
    LayoutUnit logicalRight() const { return m_rect.right(); }
    LayoutUnit logicalTop() const { return m_rect.top(); }
    LayoutUnit logicalBottom() const { return m_rect.bottom(); }

    LayoutUnit logicalWidth() const { return m_rect.width(); }
    LayoutUnit logicalHeight() const { return m_rect.height(); }

    void setBaseline(Baseline);
    const Baseline& baseline() const;
    Baseline& baseline();
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
    LayoutUnit baselineOffset() const;
    void setBaselineOffset(LayoutUnit);

    void setLogicalTopLeft(LayoutPoint logicalTopLeft) { m_rect.setTopLeft(logicalTopLeft); }
    void setLogicalHeight(LayoutUnit logicalHeight) { m_rect.setHeight(logicalHeight); }

    enum class AdjustBaseline { No, Yes };
    void setLogicalHeightIfGreater(LayoutUnit logicalHeight, AdjustBaseline);
    void setLogicalWidth(LayoutUnit logicalWidth) { m_rect.setWidth(logicalWidth); }

    void moveHorizontally(LayoutUnit delta) { m_rect.moveHorizontally(delta); }

    void expandHorizontally(LayoutUnit delta) { m_rect.expandHorizontally(delta); }
    void shrinkHorizontally(LayoutUnit delta) { expandHorizontally(-delta); }

    void expandVertically(LayoutUnit delta) { m_rect.expandVertically(delta); }
    void shrinkVertically(LayoutUnit delta) { expandVertically(-delta); }

private:
#if !ASSERT_DISABLED
    bool m_hasValidBaseline { false };
    bool m_hasValidBaselineOffset { false };
#endif
    Display::Rect m_rect;
    Baseline m_baseline;
    LayoutUnit m_baselineOffset;
};

inline LineBox::LineBox(Display::Rect rect, const Baseline& baseline, LayoutUnit baselineOffset)
    : m_rect(rect)
    , m_baseline(baseline)
    , m_baselineOffset(baselineOffset)
{
#if !ASSERT_DISABLED
    m_hasValidBaseline = true;
    m_hasValidBaselineOffset = true;
#endif
}

inline void LineBox::setLogicalHeightIfGreater(LayoutUnit logicalHeight, AdjustBaseline adjustBaseline)
{
    auto diff = logicalHeight - m_rect.height();
    if (diff <= 0)
        return;
    if (adjustBaseline == AdjustBaseline::Yes)
        setBaselineOffset(baselineOffset() + diff);
    m_rect.setHeight(logicalHeight);
}

inline void LineBox::setBaseline(Baseline baseline)
{
#if !ASSERT_DISABLED
    m_hasValidBaseline = true;
#endif
    m_baseline = baseline;
}

inline const LineBox::Baseline& LineBox::baseline() const
{
    ASSERT(m_hasValidBaseline);
    return m_baseline;
}

inline LineBox::Baseline& LineBox::baseline()
{
    ASSERT(m_hasValidBaseline);
    return m_baseline;
}

inline void LineBox::setBaselineOffset(LayoutUnit baselineOffset)
{
#if !ASSERT_DISABLED
    m_hasValidBaselineOffset = true;
#endif
    m_baselineOffset = baselineOffset;
}

inline LayoutUnit LineBox::baselineOffset() const
{
    ASSERT(m_hasValidBaselineOffset);
    return m_baselineOffset;
}

inline LineBox::Baseline::Baseline(LayoutUnit ascent, LayoutUnit descent)
    : m_ascent(ascent)
    , m_descent(descent)
{
#if !ASSERT_DISABLED
    m_hasValidAscent = true;
    m_hasValidDescent = true;
#endif
}

inline void LineBox::Baseline::setAscent(LayoutUnit ascent)
{
#if !ASSERT_DISABLED
    m_hasValidAscent = true;
#endif
    m_ascent = ascent;
}

inline void LineBox::Baseline::setDescent(LayoutUnit descent)
{
#if !ASSERT_DISABLED
    m_hasValidDescent = true;
#endif
    m_descent = descent;
}

inline void LineBox::Baseline::setAscentIfGreater(LayoutUnit ascent)
{
#if !ASSERT_DISABLED
    m_hasValidAscent = true;
#endif
    m_ascent = std::max(ascent, m_ascent);
}

inline void LineBox::Baseline::setDescentIfGreater(LayoutUnit descent)
{
#if !ASSERT_DISABLED
    m_hasValidDescent = true;
#endif
    m_descent = std::max(descent, m_descent);
}

inline void LineBox::Baseline::reset()
{
#if !ASSERT_DISABLED
    m_hasValidAscent = true;
    m_hasValidDescent = true;
#endif
    m_ascent = { };
    m_descent = { };
}

inline LayoutUnit LineBox::Baseline::ascent() const
{
    ASSERT(m_hasValidAscent);
    return m_ascent;
}

inline LayoutUnit LineBox::Baseline::descent() const
{
    ASSERT(m_hasValidDescent);
    return m_descent;
}

}
}

#endif
