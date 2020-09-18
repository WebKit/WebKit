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

#include "InlineLine.h"
#include "InlineRect.h"
#include "LayoutBoxGeometry.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace Layout {

class InlineFormattingContext;
class LineBoxBuilder;

// LineBox contains all the inline boxes both horizontally and vertically. It only has width and height geometry.
//
//   ____________________________________________________________ Line Box
// |                                    --------------
// |                                   |              |
// | ----------------------------------|--------------|---------- Root Inline Box
// ||   _____    ___      ___          |              |
// ||  |        /   \    /   \         |  Inline Box  |
// ||  |_____  |     |  |     |        |              |    ascent
// ||  |       |     |  |     |        |              |
// ||__|________\___/____\___/_________|______________|_______ alignment_baseline
// ||
// ||                                                      descent
// ||_______________________________________________________________
// |________________________________________________________________
class LineBox {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct InlineBox {
        WTF_MAKE_ISO_ALLOCATED_INLINE(InlineBox);
    public:
        InlineBox(const Box&, const InlineRect&, InlineLayoutUnit baseline);
        InlineBox(const Box&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
        InlineBox() = default;

        const InlineRect& logicalRect() const { return m_logicalRect; }
        InlineLayoutUnit logicalTop() const { return m_logicalRect.top(); }
        InlineLayoutUnit logicalBottom() const { return m_logicalRect.bottom(); }
        InlineLayoutUnit logicalLeft() const { return m_logicalRect.left(); }
        InlineLayoutUnit logicalWidth() const { return m_logicalRect.width(); }
        InlineLayoutUnit logicalHeight() const { return m_logicalRect.height(); }

        InlineLayoutUnit baseline() const { return m_baseline; }
        Optional<InlineLayoutUnit> descent() const { return m_descent; }

        bool isEmpty() const { return m_isEmpty; }
        void setIsNonEmpty() { m_isEmpty = false; }

        Optional<InlineLayoutUnit> lineSpacing() const { return m_lineSpacing; }
        const FontMetrics& fontMetrics() const { return layoutBox().style().fontMetrics(); }
        const Box& layoutBox() const { return *m_layoutBox; }

    private:
        friend class LineBoxBuilder;

        void setLogicalTop(InlineLayoutUnit logicalTop) { m_logicalRect.setTop(logicalTop); }
        void setLogicalWidth(InlineLayoutUnit logicalWidth) { m_logicalRect.setWidth(logicalWidth); }
        void setLogicalHeight(InlineLayoutUnit logicalHeight) { m_logicalRect.setHeight(logicalHeight); }
        void setBaseline(InlineLayoutUnit baseline) { m_baseline = baseline; }
        void setDescent(InlineLayoutUnit descent) { m_descent = descent; }
        void setLineSpacing(InlineLayoutUnit lineSpacing) { m_lineSpacing = lineSpacing; }

        WeakPtr<const Box> m_layoutBox;
        InlineRect m_logicalRect;
        InlineLayoutUnit m_baseline { 0 };
        Optional<InlineLayoutUnit> m_descent;
        Optional<InlineLayoutUnit> m_lineSpacing;
        bool m_isEmpty { true };
    };

    enum class IsLineVisuallyEmpty { No, Yes };
    LineBox(InlineLayoutUnit logicalWidth, IsLineVisuallyEmpty);

    InlineLayoutUnit logicalWidth() const { return m_logicalSize.width(); }
    InlineLayoutUnit logicalHeight() const { return m_logicalSize.height(); }
    InlineLayoutSize logicalSize() const { return m_logicalSize; }

    Optional<InlineLayoutUnit> horizontalAlignmentOffset() const { return m_horizontalAlignmentOffset; }
    bool isLineVisuallyEmpty() const { return m_isLineVisuallyEmpty; }

    const InlineBox& inlineBoxForLayoutBox(const Box& layoutBox) const { return *m_inlineBoxRectMap.get(&layoutBox); }
    InlineRect logicalRectForTextRun(const Line::Run&) const;

    using InlineBoxMap = HashMap<const Box*, InlineBox*>;
    auto inlineBoxList() const { return m_inlineBoxRectMap.values(); }

    InlineLayoutUnit alignmentBaseline() const { return m_rootInlineBox->logicalTop() + m_rootInlineBox->baseline(); }

private:
    friend class LineBoxBuilder;

    void setLogicalHeight(InlineLayoutUnit logicalHeight) { m_logicalSize.setHeight(logicalHeight); }
    void setHorizontalAlignmentOffset(InlineLayoutUnit horizontalAlignmentOffset) { m_horizontalAlignmentOffset = horizontalAlignmentOffset; }

    void addRootInlineBox(std::unique_ptr<InlineBox>&&);
    void addInlineBox(std::unique_ptr<InlineBox>&&);

    InlineBox& rootInlineBox() { return *m_rootInlineBox; }
    using InlineBoxList = Vector<std::unique_ptr<InlineBox>>;
    const InlineBoxList& nonRootInlineBoxes() const { return m_nonRootInlineBoxList; }

    InlineBox& inlineBoxForLayoutBox(const Box& layoutBox) { return *m_inlineBoxRectMap.get(&layoutBox); }

private:
    InlineLayoutSize m_logicalSize;
    Optional<InlineLayoutUnit> m_horizontalAlignmentOffset;
    bool m_isLineVisuallyEmpty { true };

    std::unique_ptr<InlineBox> m_rootInlineBox;
    InlineBoxList m_nonRootInlineBoxList;

    InlineBoxMap m_inlineBoxRectMap;
};

}
}

#endif
