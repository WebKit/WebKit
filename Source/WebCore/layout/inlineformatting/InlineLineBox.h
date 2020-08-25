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

struct AscentAndDescent {
    InlineLayoutUnit height() const { return ascent + descent; }

    InlineLayoutUnit ascent { 0 };
    InlineLayoutUnit descent { 0 };
};

class LineBox {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct InlineBox {
        InlineBox(const AscentAndDescent&);
        InlineBox() = default;

        AscentAndDescent ascentAndDescent;
    };

    LineBox(const Display::InlineRect&, const AscentAndDescent&);
    LineBox() = default;

    const Display::InlineRect& logicalRect() const { return m_rect; }
    const Display::InlineRect& scrollableOverflow() const { return m_scrollableOverflow; }

    InlineLayoutUnit logicalLeft() const { return m_rect.left(); }
    InlineLayoutUnit logicalRight() const { return m_rect.right(); }
    InlineLayoutUnit logicalTop() const { return m_rect.top(); }
    InlineLayoutUnit logicalBottom() const { return m_rect.bottom(); }

    InlineLayoutUnit logicalWidth() const { return m_rect.width(); }
    InlineLayoutUnit logicalHeight() const { return m_rect.height(); }

    // Aligment baseline from line logical top.
    //
    // -------------------    line logical top     ------------------- (top align)
    //             ^                                              ^
    //             |                                  ^           |
    //   ^         | alignment baseline               |           | alignment baseline
    //   |         |                                  |           |
    //   | ascent  |                                  | ascent    |
    //   |         |                                  v           v
    //   v         v                               ------------------- baseline
    //   ----------------- baseline                   ^
    //   ^                                            | descent
    //   | descent                                    v
    //   v
    // -------------------    line logical bottom  -------------------
    InlineLayoutUnit alignmentBaseline() const;
    const AscentAndDescent& ascentAndDescent() const { return m_rootInlineBox.ascentAndDescent; }

    void setAlignmentBaselineIfGreater(InlineLayoutUnit);
    void setAscentIfGreater(InlineLayoutUnit);
    void setDescentIfGreater(InlineLayoutUnit);

    void resetAlignmentBaseline();
    void resetDescent() { m_rootInlineBox.ascentAndDescent.descent = { }; }

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
    bool m_hasValidAlignmentBaseline { false };
#endif
    Display::InlineRect m_rect;
    Display::InlineRect m_scrollableOverflow;
    InlineLayoutUnit m_alignmentBaseline { 0 };
    bool m_isConsideredEmpty { true };
    InlineBox m_rootInlineBox;
};

inline LineBox::LineBox(const Display::InlineRect& rect, const AscentAndDescent& ascentAndDescent)
    : m_rect(rect)
    , m_alignmentBaseline(ascentAndDescent.ascent)
    , m_rootInlineBox(ascentAndDescent)
{
#if ASSERT_ENABLED
    m_hasValidAlignmentBaseline = true;
#endif
}

inline LineBox::InlineBox::InlineBox(const AscentAndDescent& ascentAndDescent)
    : ascentAndDescent(ascentAndDescent)
{
}

inline void LineBox::setLogicalHeightIfGreater(InlineLayoutUnit logicalHeight)
{
    if (logicalHeight <= m_rect.height())
        return;
    m_rect.setHeight(logicalHeight);
}

inline void LineBox::setAlignmentBaselineIfGreater(InlineLayoutUnit alignmentBaseline)
{
#if ASSERT_ENABLED
    m_hasValidAlignmentBaseline = true;
#endif
    m_alignmentBaseline = std::max(alignmentBaseline, m_alignmentBaseline);
}

inline void LineBox::setAscentIfGreater(InlineLayoutUnit ascent)
{
    if (ascent < m_rootInlineBox.ascentAndDescent.ascent)
        return;
    setAlignmentBaselineIfGreater(ascent);
    m_rootInlineBox.ascentAndDescent.ascent = ascent;
}

inline void LineBox::setDescentIfGreater(InlineLayoutUnit descent)
{
    if (descent < m_rootInlineBox.ascentAndDescent.descent)
        return;
    m_rootInlineBox.ascentAndDescent.descent = descent;
}

inline InlineLayoutUnit LineBox::alignmentBaseline() const
{
    ASSERT(m_hasValidAlignmentBaseline);
    return m_alignmentBaseline;
}

inline void LineBox::resetAlignmentBaseline()
{
#if ASSERT_ENABLED
    m_hasValidAlignmentBaseline = true;
#endif
    m_alignmentBaseline = 0_lu;
    m_rootInlineBox.ascentAndDescent.descent = { };
    m_rootInlineBox.ascentAndDescent.ascent = { };
}

}
}

#endif
