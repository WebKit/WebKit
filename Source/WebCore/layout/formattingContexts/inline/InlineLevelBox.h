/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "InlineRect.h"
#include "LayoutBox.h"
#include "LayoutUnits.h"
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace Layout {

class LineBox;
class LineBoxBuilder;
class LineBoxVerticalAligner;

class InlineLevelBox {
public:
    enum class LineSpanningInlineBox { Yes, No };
    static InlineLevelBox createInlineBox(const Box&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, LineSpanningInlineBox = LineSpanningInlineBox::No);
    static InlineLevelBox createAtomicInlineLevelBox(const Box&, InlineLayoutUnit logicalLeft, InlineLayoutSize);
    static InlineLevelBox createLineBreakBox(const Box&, InlineLayoutUnit logicalLeft);
    static InlineLevelBox createGenericInlineLevelBox(const Box&, InlineLayoutUnit logicalLeft);

    InlineLayoutUnit baseline() const { return m_baseline; }
    std::optional<InlineLayoutUnit> descent() const { return m_descent; }
    // See https://www.w3.org/TR/css-inline-3/#layout-bounds
    struct LayoutBounds {
        InlineLayoutUnit height() const { return ascent + descent; }
        bool operator==(const LayoutBounds& other) const { return ascent == other.ascent && descent == other.descent; }

        InlineLayoutUnit ascent { 0 };
        InlineLayoutUnit descent { 0 };
    };
    LayoutBounds layoutBounds() const { return m_layoutBounds; }

    bool hasContent() const { return m_hasContent; }
    void setHasContent();

    VerticalAlign verticalAlign() const { return layoutBox().style().verticalAlign(); }
    const Box& layoutBox() const { return *m_layoutBox; }
    const RenderStyle& style() const { return m_layoutBox->style(); }

    bool isInlineBox() const { return m_type == Type::InlineBox || isRootInlineBox() || isLineSpanningInlineBox(); }
    bool isRootInlineBox() const { return m_type == Type::RootInlineBox; }
    bool isLineSpanningInlineBox() const { return m_type == Type::LineSpanningInlineBox; }
    bool isAtomicInlineLevelBox() const { return m_type == Type::AtomicInlineLevelBox; }
    bool isLineBreakBox() const { return m_type == Type::LineBreakBox; }
    bool hasLineBoxRelativeAlignment() const;

    enum class Type : uint8_t {
        InlineBox             = 1 << 0,
        LineSpanningInlineBox = 1 << 1,
        RootInlineBox         = 1 << 1,
        AtomicInlineLevelBox  = 1 << 2,
        LineBreakBox          = 1 << 3,
        GenericInlineLevelBox = 1 << 4
    };
    Type type() const { return m_type; }

    InlineLevelBox(const Box&, InlineLayoutUnit logicalLeft, InlineLayoutSize, Type);
    InlineLevelBox() = default;

private:
    friend class LineBox;
    friend class LineBoxBuilder;
    friend class LineBoxVerticalAligner;

    const InlineRect& logicalRect() const { return m_logicalRect; }
    InlineLayoutUnit logicalTop() const { return m_logicalRect.top(); }
    InlineLayoutUnit logicalBottom() const { return m_logicalRect.bottom(); }
    InlineLayoutUnit logicalLeft() const { return m_logicalRect.left(); }
    InlineLayoutUnit logicalWidth() const { return m_logicalRect.width(); }
    InlineLayoutUnit logicalHeight() const { return m_logicalRect.height(); }

    // FIXME: Remove legacy rounding.
    void setLogicalWidth(InlineLayoutUnit logicalWidth) { m_logicalRect.setWidth(logicalWidth); }
    void setLogicalHeight(InlineLayoutUnit logicalHeight) { m_logicalRect.setHeight(roundToInt(logicalHeight)); }
    void setLogicalTop(InlineLayoutUnit logicalTop) { m_logicalRect.setTop(roundToInt(logicalTop)); }
    void setBaseline(InlineLayoutUnit baseline) { m_baseline = roundToInt(baseline); }
    void setDescent(InlineLayoutUnit descent) { m_descent = roundToInt(descent); }
    void setLayoutBounds(const LayoutBounds& layoutBounds) { m_layoutBounds = { InlineLayoutUnit(roundToInt(layoutBounds.ascent)), InlineLayoutUnit(roundToInt(layoutBounds.descent)) }; }

private:
    WeakPtr<const Box> m_layoutBox;
    // This is the combination of margin and border boxes. Inline level boxes are vertically aligned using their margin boxes.
    InlineRect m_logicalRect;
    LayoutBounds m_layoutBounds;
    InlineLayoutUnit m_baseline { 0 };
    std::optional<InlineLayoutUnit> m_descent;
    bool m_hasContent { false };
    Type m_type { Type::InlineBox };
};

inline InlineLevelBox::InlineLevelBox(const Box& layoutBox, InlineLayoutUnit logicalLeft, InlineLayoutSize logicalSize, Type type)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_logicalRect({ }, logicalLeft, logicalSize.width(), logicalSize.height())
    , m_type(type)
{
}

inline void InlineLevelBox::setHasContent()
{
    ASSERT(isInlineBox());
    m_hasContent = true;
}

inline bool InlineLevelBox::hasLineBoxRelativeAlignment() const
{
    auto verticalAlignment = layoutBox().style().verticalAlign();
    return verticalAlignment == VerticalAlign::Top || verticalAlignment == VerticalAlign::Bottom;
}

inline InlineLevelBox InlineLevelBox::createAtomicInlineLevelBox(const Box& layoutBox, InlineLayoutUnit logicalLeft, InlineLayoutSize logicalSize)
{
    return InlineLevelBox { layoutBox, logicalLeft, logicalSize, Type::AtomicInlineLevelBox };
}

inline InlineLevelBox InlineLevelBox::createInlineBox(const Box& layoutBox, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, LineSpanningInlineBox isLineSpanning)
{
    return InlineLevelBox { layoutBox, logicalLeft, InlineLayoutSize { logicalWidth, { } }, isLineSpanning == LineSpanningInlineBox::Yes ? Type::LineSpanningInlineBox : Type::InlineBox };
}

inline InlineLevelBox InlineLevelBox::createLineBreakBox(const Box& layoutBox, InlineLayoutUnit logicalLeft)
{
    return InlineLevelBox { layoutBox, logicalLeft, InlineLayoutSize { }, Type::LineBreakBox };
}

inline InlineLevelBox InlineLevelBox::createGenericInlineLevelBox(const Box& layoutBox, InlineLayoutUnit logicalLeft)
{
    return InlineLevelBox { layoutBox, logicalLeft, InlineLayoutSize { }, Type::GenericInlineLevelBox };
}

}
}

#endif
