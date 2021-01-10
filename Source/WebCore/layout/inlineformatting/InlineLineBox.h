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
#include <wtf/IsoMallocInlines.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace Layout {

class BoxGeometry;
class InlineFormattingContext;
class LineBoxBuilder;

//   ____________________________________________________________ Line Box
// |                                    --------------------
// |                                   |                    |
// | ----------------------------------|--------------------|---------- Root Inline Box
// ||   _____    ___      ___          |                    |
// ||  |        /   \    /   \         |  Inline Level Box  |
// ||  |_____  |     |  |     |        |                    |    ascent
// ||  |       |     |  |     |        |                    |
// ||__|________\___/____\___/_________|____________________|_______ alignment_baseline
// ||
// ||                                                      descent
// ||_______________________________________________________________
// |________________________________________________________________
// The resulting rectangular area that contains the boxes that form a single line of inline-level content is called a line box.
// https://www.w3.org/TR/css-inline-3/#model
class LineBox {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct InlineLevelBox {
        WTF_MAKE_ISO_ALLOCATED_INLINE(InlineLevelBox);
    public:
        static std::unique_ptr<LineBox::InlineLevelBox> createRootInlineBox(const Box&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
        static std::unique_ptr<LineBox::InlineLevelBox> createInlineBox(const Box&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
        static std::unique_ptr<LineBox::InlineLevelBox> createAtomicInlineLevelBox(const Box&, InlineLayoutUnit logicalLeft, InlineLayoutSize);
        static std::unique_ptr<LineBox::InlineLevelBox> createLineBreakBox(const Box&, InlineLayoutUnit logicalLeft);
        static std::unique_ptr<LineBox::InlineLevelBox> createGenericInlineLevelBox(const Box&, InlineLayoutUnit logicalLeft);

        InlineLayoutUnit baseline() const { return m_baseline; }
        Optional<InlineLayoutUnit> descent() const { return m_descent; }
        // See https://www.w3.org/TR/css-inline-3/#layout-bounds
        struct LayoutBounds {
            InlineLayoutUnit height() const { return ascent + descent; }

            InlineLayoutUnit ascent { 0 };
            InlineLayoutUnit descent { 0 };
        };
        LayoutBounds layoutBounds() const { return m_layoutBounds; }

        bool hasContent() const { return m_hasContent; }
        void setHasContent();

        VerticalAlign verticalAlign() const { return layoutBox().style().verticalAlign(); }
        const Box& layoutBox() const { return *m_layoutBox; }
        const RenderStyle& style() const { return m_layoutBox->style(); }

        bool isInlineBox() const { return m_type == Type::InlineBox || m_type == Type::RootInlineBox; }
        bool isRootInlineBox() const { return m_type == Type::RootInlineBox; }
        bool isAtomicInlineLevelBox() const { return m_type == Type::AtomicInlineLevelBox; }
        bool isLineBreakBox() const { return m_type == Type::LineBreakBox; }
        bool hasLineBoxRelativeAlignment() const;

        enum class Type {
            InlineBox,
            RootInlineBox,
            AtomicInlineLevelBox,
            LineBreakBox,
            GenericInlineLevelBox
        };
        InlineLevelBox(const Box&, InlineLayoutUnit logicalLeft, InlineLayoutSize, Type);
        InlineLevelBox() = default;

    private:
        friend class LineBoxBuilder;
        friend class LineBox;

        const InlineRect& logicalRect() const { return m_logicalRect; }
        InlineLayoutUnit logicalTop() const { return m_logicalRect.top(); }
        InlineLayoutUnit logicalBottom() const { return m_logicalRect.bottom(); }
        InlineLayoutUnit logicalLeft() const { return m_logicalRect.left(); }
        InlineLayoutUnit logicalWidth() const { return m_logicalRect.width(); }
        InlineLayoutUnit logicalHeight() const { return m_logicalRect.height(); }

        void setLogicalWidth(InlineLayoutUnit logicalWidth) { m_logicalRect.setWidth(logicalWidth); }
        void setLogicalHeight(InlineLayoutUnit);
        void setLogicalTop(InlineLayoutUnit);
        void setBaseline(InlineLayoutUnit);
        void setDescent(InlineLayoutUnit);
        void setLayoutBounds(const LayoutBounds&);

    private:
        WeakPtr<const Box> m_layoutBox;
        InlineRect m_logicalRect;
        LayoutBounds m_layoutBounds;
        InlineLayoutUnit m_baseline { 0 };
        Optional<InlineLayoutUnit> m_descent;
        bool m_hasContent { false };
        Type m_type { Type::InlineBox };
    };

    enum class IsLineConsideredEmpty { No, Yes };
    LineBox(const InlineLayoutPoint& logicalTopLeft, InlineLayoutUnit logicalWidth, IsLineConsideredEmpty, size_t numberOfRuns);

    const InlineRect& logicalRect() const { return m_logicalRect; }
    InlineLayoutUnit logicalWidth() const { return logicalSize().width(); }
    InlineLayoutUnit logicalHeight() const { return logicalSize().height(); }
    InlineLayoutPoint logicalTopLeft() const { return logicalRect().topLeft(); }
    InlineLayoutSize logicalSize() const { return logicalRect().size(); }

    Optional<InlineLayoutUnit> horizontalAlignmentOffset() const { return m_horizontalAlignmentOffset; }
    bool isConsideredEmpty() const { return m_isConsideredEmpty; }
    bool hasInlineBox() const { return m_hasInlineBox; }

    const InlineLevelBox& inlineLevelBoxForLayoutBox(const Box& layoutBox) const { return *m_inlineLevelBoxRectMap.get(&layoutBox); }

    InlineRect logicalRectForTextRun(const Line::Run&) const;
    InlineRect logicalRectForRootInlineBox() const { return m_rootInlineBox->logicalRect(); }
    InlineRect logicalMarginRectForInlineLevelBox(const Box&, const BoxGeometry&) const;

    const InlineLevelBox& rootInlineBox() const { return *m_rootInlineBox; }
    using InlineLevelBoxList = Vector<std::unique_ptr<InlineLevelBox>>;
    const InlineLevelBoxList& nonRootInlineLevelBoxes() const { return m_nonRootInlineLevelBoxList; }

    InlineLayoutUnit alignmentBaseline() const { return m_rootInlineBox->logicalTop() + m_rootInlineBox->baseline(); }

private:
    friend class LineBoxBuilder;

    void setLogicalHeight(InlineLayoutUnit logicalHeight) { m_logicalRect.setHeight(logicalHeight); }
    void setHorizontalAlignmentOffset(InlineLayoutUnit horizontalAlignmentOffset) { m_horizontalAlignmentOffset = horizontalAlignmentOffset; }

    void addRootInlineBox(std::unique_ptr<InlineLevelBox>&&);
    void addInlineLevelBox(std::unique_ptr<InlineLevelBox>&&);

    InlineLevelBox& rootInlineBox() { return *m_rootInlineBox; }

    InlineLevelBox& inlineLevelBoxForLayoutBox(const Box& layoutBox) { return *m_inlineLevelBoxRectMap.get(&layoutBox); }

private:
    InlineRect m_logicalRect;
    Optional<InlineLayoutUnit> m_horizontalAlignmentOffset;
    bool m_isConsideredEmpty { true };
    bool m_hasInlineBox { false };

    std::unique_ptr<InlineLevelBox> m_rootInlineBox;
    InlineLevelBoxList m_nonRootInlineLevelBoxList;

    HashMap<const Box*, InlineLevelBox*> m_inlineLevelBoxRectMap;
};

inline std::unique_ptr<LineBox::InlineLevelBox> LineBox::InlineLevelBox::createRootInlineBox(const Box& layoutBox, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
{
    return makeUnique<LineBox::InlineLevelBox>(layoutBox, logicalLeft, InlineLayoutSize { logicalWidth, { } }, Type::RootInlineBox);
}

inline std::unique_ptr<LineBox::InlineLevelBox> LineBox::InlineLevelBox::createAtomicInlineLevelBox(const Box& layoutBox, InlineLayoutUnit logicalLeft, InlineLayoutSize logicalSize)
{
    return makeUnique<LineBox::InlineLevelBox>(layoutBox, logicalLeft, logicalSize, Type::AtomicInlineLevelBox);
}

inline std::unique_ptr<LineBox::InlineLevelBox> LineBox::InlineLevelBox::createInlineBox(const Box& layoutBox, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
{
    return makeUnique<LineBox::InlineLevelBox>(layoutBox, logicalLeft, InlineLayoutSize { logicalWidth, { } }, Type::InlineBox);
}

inline std::unique_ptr<LineBox::InlineLevelBox> LineBox::InlineLevelBox::createLineBreakBox(const Box& layoutBox, InlineLayoutUnit logicalLeft)
{
    return makeUnique<LineBox::InlineLevelBox>(layoutBox, logicalLeft, InlineLayoutSize { }, Type::LineBreakBox);
}

inline std::unique_ptr<LineBox::InlineLevelBox> LineBox::InlineLevelBox::createGenericInlineLevelBox(const Box& layoutBox, InlineLayoutUnit logicalLeft)
{
    return makeUnique<LineBox::InlineLevelBox>(layoutBox, logicalLeft, InlineLayoutSize { }, Type::GenericInlineLevelBox);
}

}
}

#endif
