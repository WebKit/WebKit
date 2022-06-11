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

#include "InlineRect.h"
#include "LayoutBox.h"
#include "TextFlags.h"
#include <unicode/ubidi.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace InlineDisplay {

struct Box {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    struct Text {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
    public:
        Text(size_t position, size_t length, const String& originalContent, String adjustedContentToRender = String(), bool hasHyphen = false);

        size_t start() const { return m_start; }
        size_t end() const { return start() + length(); }
        size_t length() const { return m_length; }
        StringView originalContent() const { return StringView(m_originalContent).substring(m_start, m_length); }
        StringView renderedContent() const { return m_adjustedContentToRender.isNull() ? originalContent() : m_adjustedContentToRender; }

        bool hasHyphen() const { return m_hasHyphen; }

    private:
        size_t m_start { 0 };
        size_t m_length { 0 };
        bool m_hasHyphen { false };
        String m_originalContent;
        String m_adjustedContentToRender;
    };

    enum class Type {
        Text,
        WordSeparator,
        Ellipsis,
        SoftLineBreak,
        LineBreakBox,
        AtomicInlineLevelBox,
        NonRootInlineBox,
        RootInlineBox,
        GenericInlineLevelBox
    };
    struct Expansion;
    enum class PositionWithinInlineLevelBox {
        First = 1 << 0,
        Last  = 1 << 1
    };
    Box(size_t lineIndex, Type, const Layout::Box&, UBiDiLevel, const FloatRect&, const FloatRect& inkOverflow, Expansion, std::optional<Text> = std::nullopt, bool hasContent = true, OptionSet<PositionWithinInlineLevelBox> = { });

    bool isText() const { return m_type == Type::Text || isWordSeparator(); }
    bool isWordSeparator() const { return m_type == Type::WordSeparator; }
    bool isEllipsis() const { return m_type == Type::Ellipsis; }
    bool isSoftLineBreak() const { return m_type == Type::SoftLineBreak; }
    bool isTextOrSoftLineBreak() const { return isText() || isSoftLineBreak(); }
    bool isLineBreakBox() const { return m_type == Type::LineBreakBox; }
    bool isLineBreak() const { return isSoftLineBreak() || isLineBreakBox(); }
    bool isAtomicInlineLevelBox() const { return m_type == Type::AtomicInlineLevelBox; }
    bool isInlineBox() const { return isNonRootInlineBox() || isRootInlineBox(); }
    bool isNonRootInlineBox() const { return m_type == Type::NonRootInlineBox; }
    bool isRootInlineBox() const { return m_type == Type::RootInlineBox; }
    bool isGenericInlineLevelBox() const { return m_type == Type::GenericInlineLevelBox; }
    bool isInlineLevelBox() const { return isAtomicInlineLevelBox() || isLineBreakBox() || isInlineBox() || isGenericInlineLevelBox(); }
    bool isNonRootInlineLevelBox() const { return isInlineLevelBox() && !isRootInlineBox(); }

    UBiDiLevel bidiLevel() const { return m_bidiLevel; }

    bool isHorizontal() const { return style().isHorizontalWritingMode(); }

    bool hasContent() const { return m_hasContent; }

    const FloatRect& visualRectIgnoringBlockDirection() const { return m_unflippedVisualRect; }
    const FloatRect& inkOverflow() const { return m_inkOverflow; }

    float top() const { return visualRectIgnoringBlockDirection().y(); }
    float bottom() const { return visualRectIgnoringBlockDirection().maxY(); }
    float left() const { return visualRectIgnoringBlockDirection().x(); }
    float right() const { return visualRectIgnoringBlockDirection().maxX(); }

    float width() const { return visualRectIgnoringBlockDirection().width(); }
    float height() const { return visualRectIgnoringBlockDirection().height(); }

    void moveVertically(float offset)
    {
        m_unflippedVisualRect.move({ { }, offset });
        m_inkOverflow.move({ { }, offset });
    }
    void moveHorizontally(float offset)
    {
        m_unflippedVisualRect.move({ offset, { } });
        m_inkOverflow.move({ offset, { } });
    }
    void adjustInkOverflow(const FloatRect& childBorderBox) { return m_inkOverflow.uniteEvenIfEmpty(childBorderBox); }
    void truncate(float truncatedwidth = 0.f);
    void setLeft(float pysicalLeft)
    {
        auto offset = pysicalLeft - left();
        m_unflippedVisualRect.setX(pysicalLeft);
        m_inkOverflow.setX(m_inkOverflow.x() + offset);
    }
    void setRight(float physicalRight)
    {
        auto offset = physicalRight - right();
        m_unflippedVisualRect.shiftMaxXEdgeTo(physicalRight);
        m_inkOverflow.shiftMaxXEdgeTo(m_inkOverflow.maxX() + offset);
    }
    void setTop(float physicalTop)
    {
        auto offset = physicalTop - top();
        m_unflippedVisualRect.setY(physicalTop);
        m_inkOverflow.setY(m_inkOverflow.y() + offset);
    }
    void setBottom(float physicalBottom)
    {
        auto offset = physicalBottom - bottom();
        m_unflippedVisualRect.shiftMaxYEdgeTo(physicalBottom);
        m_inkOverflow.shiftMaxYEdgeTo(m_inkOverflow.maxY() + offset);
    }
    void setRect(const FloatRect& rect, const FloatRect& inkOverflow)
    {
        m_unflippedVisualRect = rect;
        m_inkOverflow = inkOverflow;
    }
    void setHasContent() { m_hasContent = true; }

    std::optional<Text>& text() { return m_text; }
    const std::optional<Text>& text() const { return m_text; }

    struct Expansion {
        ExpansionBehavior behavior = ExpansionBehavior::defaultBehavior();
        float horizontalExpansion { 0 };
    };
    Expansion expansion() const { return m_expansion; }

    const Layout::Box& layoutBox() const { return m_layoutBox; }
    const RenderStyle& style() const { return !lineIndex() ? layoutBox().firstLineStyle() : layoutBox().style(); }

    size_t lineIndex() const { return m_lineIndex; }
    // These functions tell you whether this display box is the first/last for the associated inline level box (Layout::Box) and not whether it's the first/last box on the line.
    // (e.g. always true for atomic boxes, but inline boxes spanning over multiple lines can produce individual first/last boxes).
    bool isFirstForLayoutBox() const { return m_isFirstForLayoutBox; }
    bool isLastForLayoutBox() const { return m_isLastForLayoutBox; }

    void setIsFirstForLayoutBox(bool isFirstBox) { m_isFirstForLayoutBox = isFirstBox; }
    void setIsLastForLayoutBox(bool isLastBox) { m_isLastForLayoutBox = isLastBox; }

private:
    const size_t m_lineIndex { 0 };
    const Type m_type { Type::GenericInlineLevelBox };
    CheckedRef<const Layout::Box> m_layoutBox;
    UBiDiLevel m_bidiLevel { UBIDI_DEFAULT_LTR };
    FloatRect m_unflippedVisualRect;
    FloatRect m_inkOverflow;
    bool m_hasContent : 1;
    bool m_isFirstForLayoutBox : 1;
    bool m_isLastForLayoutBox : 1;
    Expansion m_expansion;
    std::optional<Text> m_text;
};

inline Box::Box(size_t lineIndex, Type type, const Layout::Box& layoutBox, UBiDiLevel bidiLevel, const FloatRect& physicalRect, const FloatRect& inkOverflow, Expansion expansion, std::optional<Text> text, bool hasContent, OptionSet<PositionWithinInlineLevelBox> positionWithinInlineLevelBox)
    : m_lineIndex(lineIndex)
    , m_type(type)
    , m_layoutBox(layoutBox)
    , m_bidiLevel(bidiLevel)
    , m_unflippedVisualRect(physicalRect)
    , m_inkOverflow(inkOverflow)
    , m_hasContent(hasContent)
    , m_isFirstForLayoutBox(positionWithinInlineLevelBox.contains(PositionWithinInlineLevelBox::First))
    , m_isLastForLayoutBox(positionWithinInlineLevelBox.contains(PositionWithinInlineLevelBox::Last))
    , m_expansion(expansion)
    , m_text(text)
{
}

inline Box::Text::Text(size_t start, size_t length, const String& originalContent, String adjustedContentToRender, bool hasHyphen)
    : m_start(start)
    , m_length(length)
    , m_hasHyphen(hasHyphen)
    , m_originalContent(originalContent)
    , m_adjustedContentToRender(adjustedContentToRender)
{
}

inline void Box::truncate(float truncatedwidth)
{
    m_unflippedVisualRect.setWidth(truncatedwidth);
    m_inkOverflow.shiftMaxXEdgeTo(m_unflippedVisualRect.maxY());
}

}
}
#endif
