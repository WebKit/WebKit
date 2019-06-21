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
        LayoutUnit height() const { return ascent + descent; }

        LayoutUnit ascent;
        LayoutUnit descent;
    };
    LineBox(Display::Rect, const Baseline&, LayoutUnit baselineOffset);
    
    LayoutPoint logicalTopLeft() const { return m_rect.topLeft(); }

    LayoutUnit logicalLeft() const { return m_rect.left(); }
    LayoutUnit logicalRight() const { return m_rect.right(); }
    LayoutUnit logicalTop() const { return m_rect.top(); }
    LayoutUnit logicalBottom() const { return m_rect.bottom(); }

    LayoutUnit logicalWidth() const { return m_rect.width(); }
    LayoutUnit logicalHeight() const { return m_rect.height(); }

    Baseline baseline() const { return m_baseline; }
    // Baseline offset from line logical top. Note that offset does not necessarily equal to ascent.
    //
    // ------------------- line logical top
    //             ^
    //             |
    //   ^         | baseline offset
    //   |         |
    //   | ascent  |
    //   |         |
    //   v         v
    //   ----------------- baseline
    //   ^
    //   | descent
    //   v
    // ------------------- line logical bottom
    LayoutUnit baselineOffset() const { return m_baselineOffset; }

private:
    Display::Rect m_rect;
    Baseline m_baseline;
    LayoutUnit m_baselineOffset;
};

inline LineBox::LineBox(Display::Rect rect, const Baseline& baseline, LayoutUnit baselineOffset)
    : m_rect(rect)
    , m_baseline(baseline)
    , m_baselineOffset(baselineOffset)
{
}

}
}

#endif
