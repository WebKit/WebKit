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
#include "InlineLine.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace Layout {

class InlineFormattingContext;

class LineBox {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct InlineBox {
        WTF_MAKE_ISO_ALLOCATED_INLINE(InlineBox);
    public:
        enum class IsConsideredEmpty { Yes, No };
        InlineBox(const Box&, const Display::InlineRect&, InlineLayoutUnit baseline, InlineLayoutUnit descent, IsConsideredEmpty);
        InlineBox() = default;

        const Display::InlineRect& logicalRect() const { return m_logicalRect; }
        InlineLayoutUnit logicalTop() const { return m_logicalRect.top(); }
        InlineLayoutUnit logicalBottom() const { return m_logicalRect.bottom(); }
        InlineLayoutUnit logicalLeft() const { return m_logicalRect.left(); }
        InlineLayoutUnit logicalWidth() const { return m_logicalRect.width(); }
        InlineLayoutUnit logicalHeight() const { return m_logicalRect.height(); }

        InlineLayoutUnit baseline() const { return m_baseline; }
        Optional<InlineLayoutUnit> descent() const { return m_descent; }

        bool isEmpty() const { return m_isEmpty; }
        void setIsNonEmpty() { m_isEmpty = false; }

        const Box& layoutBox() const { return *m_layoutBox; }

    private:
        friend class LineBox;

        void setLogicalTop(InlineLayoutUnit logicalTop) { m_logicalRect.setTop(logicalTop); }
        void setLogicalWidth(InlineLayoutUnit logicalWidth) { m_logicalRect.setWidth(logicalWidth); }
        void setLogicalHeight(InlineLayoutUnit logicalHeight) { m_logicalRect.setHeight(logicalHeight); }
        void setBaseline(InlineLayoutUnit baseline) { m_baseline = baseline; }

        WeakPtr<const Box> m_layoutBox;
        Display::InlineRect m_logicalRect;
        InlineLayoutUnit m_baseline;
        Optional<InlineLayoutUnit> m_descent;
        bool m_isEmpty { true };
    };

    enum class IsLastLineWithInlineContent { No, Yes };
    enum class IsLineVisuallyEmpty { No, Yes };
    LineBox(const InlineFormattingContext&, const InlineLayoutPoint& topLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit contentLogicalWidth, const Line::RunList&, IsLineVisuallyEmpty, IsLastLineWithInlineContent);

    InlineLayoutUnit logicalLeft() const { return m_rect.left(); }
    InlineLayoutUnit logicalRight() const { return m_rect.right(); }
    InlineLayoutUnit logicalTop() const { return m_rect.top(); }
    InlineLayoutUnit logicalBottom() const { return m_rect.bottom(); }
    InlineLayoutUnit logicalWidth() const { return m_rect.width(); }
    InlineLayoutUnit logicalHeight() const { return m_rect.height(); }

    const Display::InlineRect& logicalRect() const { return m_rect; }
    const Display::InlineRect& scrollableOverflow() const { return m_scrollableOverflow; }

    Optional<InlineLayoutUnit> horizontalAlignmentOffset() const { return m_horizontalAlignmentOffset; }

    const InlineBox& inlineBoxForLayoutBox(const Box& layoutBox) const { return *m_inlineBoxRectMap.get(&layoutBox); }
    Display::InlineRect inlineRectForTextRun(const Line::Run&) const;

    // _____________________________________________________ line box logical top
    //                      ^
    //                      |
    //   ____________________________________ root inline box
    //   ^                  |
    //   |                  |
    //   | root line ascent |
    //   |                  |
    //   v                  v
    //   ___________________________________________________ alignment baseline
    //   ^
    //   | root line descent
    //   v
    //   ___________________________________
    // _____________________________________________________ line box logical bottom
    InlineLayoutUnit alignmentBaseline() const { return m_rootInlineBox.logicalTop() + m_rootInlineBox.baseline(); }

private:
    void constructInlineBoxes(const Line::RunList&, IsLineVisuallyEmpty);
    void computeInlineBoxesLogicalHeight();
    void alignInlineBoxesVerticallyAndComputeLineBoxHeight(IsLineVisuallyEmpty);

    InlineBox& inlineBoxForLayoutBox(const Box& layoutBox) { return *m_inlineBoxRectMap.get(&layoutBox); }

    InlineLayoutUnit contentLogicalWidth() const { return m_contentLogicalWidth; }

    const InlineFormattingContext& formattingContext() const;
    const Box& root() const;
    LayoutState& layoutState() const;

private:
    Display::InlineRect m_rect;
    Display::InlineRect m_scrollableOverflow;
    InlineLayoutUnit m_contentLogicalWidth { 0 };

    InlineBox m_rootInlineBox;

    Optional<InlineLayoutUnit> m_horizontalAlignmentOffset;
    HashMap<const Box*, InlineBox*> m_inlineBoxRectMap;
    Vector<std::unique_ptr<InlineBox>> m_inlineBoxList;
    const InlineFormattingContext& m_inlineFormattingContext;
};

}
}

#endif
