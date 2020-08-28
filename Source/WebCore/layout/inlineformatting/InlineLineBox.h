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
#include "InlineLineBuilder.h"

namespace WebCore {
namespace Layout {

class InlineFormattingContext;
struct HangingContent;

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

    enum class IsLastLineWithInlineContent { No, Yes };
    enum class IsLineVisuallyEmpty { No, Yes };
    LineBox(const InlineFormattingContext&, const Display::InlineRect&, const LineBuilder::RunList&, IsLineVisuallyEmpty, IsLastLineWithInlineContent);

    using InlineRunRectList = Vector<Display::InlineRect, 10>;
    const InlineRunRectList& inlineRectList() const { return m_runRectList; }

    InlineLayoutUnit logicalLeft() const { return m_rect.left(); }
    InlineLayoutUnit logicalRight() const { return m_rect.right(); }
    InlineLayoutUnit logicalTop() const { return m_rect.top(); }
    InlineLayoutUnit logicalBottom() const { return m_rect.bottom(); }
    InlineLayoutUnit logicalWidth() const { return m_rect.width(); }
    InlineLayoutUnit logicalHeight() const { return m_rect.height(); }

    const Display::InlineRect& logicalRect() const { return m_rect; }
    const Display::InlineRect& scrollableOverflow() const { return m_scrollableOverflow; }

    static AscentAndDescent halfLeadingMetrics(const FontMetrics&, InlineLayoutUnit lineLogicalHeight);

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

private:
    const AscentAndDescent& ascentAndDescent() const { return m_rootInlineBox.ascentAndDescent; }

    void setAlignmentBaselineIfGreater(InlineLayoutUnit);
    void setAscentIfGreater(InlineLayoutUnit);
    void setDescentIfGreater(InlineLayoutUnit);
    void setLogicalHeightIfGreater(InlineLayoutUnit);

    void setScrollableOverflow(const Display::InlineRect& rect) { m_scrollableOverflow = rect; }

    void alignHorizontally(InlineLayoutUnit availableWidth, IsLastLineWithInlineContent);
    void alignVertically();
    void adjustBaselineAndLineHeight();

    HangingContent collectHangingContent(IsLastLineWithInlineContent) const;

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    LayoutState& layoutState() const { return formattingContext().layoutState(); }

private:
#if ASSERT_ENABLED
    bool m_hasValidAlignmentBaseline { false };
#endif
    const InlineFormattingContext& m_inlineFormattingContext;
    const LineBuilder::RunList& m_runs;
    Display::InlineRect m_rect;
    Display::InlineRect m_scrollableOverflow;
    InlineLayoutUnit m_alignmentBaseline { 0 };
    InlineBox m_rootInlineBox;
    InlineRunRectList m_runRectList;
};

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

inline AscentAndDescent LineBox::halfLeadingMetrics(const FontMetrics& fontMetrics, InlineLayoutUnit lineLogicalHeight)
{
    auto ascent = fontMetrics.ascent();
    auto descent = fontMetrics.descent();
    // 10.8.1 Leading and half-leading
    auto halfLeading = (lineLogicalHeight - (ascent + descent)) / 2;
    // Inline tree height is all integer based.
    auto adjustedAscent = std::max<InlineLayoutUnit>(floorf(ascent + halfLeading), 0);
    auto adjustedDescent = std::max<InlineLayoutUnit>(ceilf(descent + halfLeading), 0);
    return { adjustedAscent, adjustedDescent };
}

}
}

#endif
