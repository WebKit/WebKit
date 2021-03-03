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

        enum class Type : uint8_t {
            InlineBox             = 1 << 0,
            RootInlineBox         = 1 << 1,
            AtomicInlineLevelBox  = 1 << 2,
            LineBreakBox          = 1 << 3,
            GenericInlineLevelBox = 1 << 4
        };
        Type type() const { return m_type; }

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
        // This is the combination of margin and border boxes. Inline level boxes are vertically aligned using their margin boxes.
        InlineRect m_logicalRect;
        LayoutBounds m_layoutBounds;
        InlineLayoutUnit m_baseline { 0 };
        Optional<InlineLayoutUnit> m_descent;
        bool m_hasContent { false };
        Type m_type { Type::InlineBox };
    };

    LineBox(const InlineLayoutPoint& logicalTopLeft, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit contentLogicalWidth, size_t numberOfRuns);

    const InlineRect& logicalRect() const { return m_logicalRect; }
    InlineLayoutUnit logicalWidth() const { return logicalSize().width(); }
    InlineLayoutUnit logicalHeight() const { return logicalSize().height(); }
    InlineLayoutPoint logicalTopLeft() const { return logicalRect().topLeft(); }
    InlineLayoutSize logicalSize() const { return logicalRect().size(); }
    InlineLayoutUnit contentLogicalWidth() const { return m_contentLogicalWidth; }

    Optional<InlineLayoutUnit> horizontalAlignmentOffset() const { return m_horizontalAlignmentOffset; }

    // Note that the line can have many inline boxes and be "empty" the same time e.g. <div><span></span><span></span></div>
    bool hasContent() const { return m_hasContent; }
    bool hasInlineBox() const { return m_boxTypes.contains(InlineLevelBox::Type::InlineBox); }
    bool hasNonInlineBox() const { return m_boxTypes.containsAny({ InlineLevelBox::Type::AtomicInlineLevelBox, InlineLevelBox::Type::LineBreakBox, InlineLevelBox::Type::GenericInlineLevelBox }); }
    bool hasAtomicInlineLevelBox() const { return m_boxTypes.contains(InlineLevelBox::Type::AtomicInlineLevelBox); }

    const InlineLevelBox& inlineLevelBoxForLayoutBox(const Box& layoutBox) const { return *m_inlineLevelBoxRectMap.get(&layoutBox); }

    InlineRect logicalRectForTextRun(const Line::Run&) const;
    InlineRect logicalRectForLineBreakBox(const Box&) const;
    InlineRect logicalRectForRootInlineBox() const { return m_rootInlineBox->logicalRect(); }
    InlineRect logicalBorderBoxForAtomicInlineLevelBox(const Box&, const BoxGeometry&) const;
    InlineRect logicalBorderBoxForInlineBox(const Box&, const BoxGeometry&) const;

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
    InlineRect logicalRectForInlineLevelBox(const Box& layoutBox) const;

    void setHasContent(bool hasContent) { m_hasContent = hasContent; }

private:
    InlineRect m_logicalRect;
    InlineLayoutUnit m_contentLogicalWidth { 0 };
    bool m_hasContent { false };
    Optional<InlineLayoutUnit> m_horizontalAlignmentOffset;
    OptionSet<InlineLevelBox::Type> m_boxTypes;

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
