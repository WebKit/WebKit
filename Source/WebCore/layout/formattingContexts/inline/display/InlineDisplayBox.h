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
    Box(size_t lineIndex, Type, const Layout::Box&, UBiDiLevel, const Layout::InlineRect&, const Layout::InlineRect& inkOverflow, Expansion, std::optional<Text> = std::nullopt, bool hasContent = true, OptionSet<PositionWithinInlineLevelBox> = { PositionWithinInlineLevelBox::First, PositionWithinInlineLevelBox::Last });

    bool isText() const { return m_type == Type::Text; }
    bool isSoftLineBreak() const { return m_type == Type::SoftLineBreak; }
    bool isLineBreakBox() const { return m_type == Type::LineBreakBox; }
    bool isLineBreak() const { return isSoftLineBreak() || isLineBreakBox(); }
    bool isAtomicInlineLevelBox() const { return m_type == Type::AtomicInlineLevelBox; }
    bool isInlineBox() const { return isNonRootInlineBox() || isRootInlineBox(); }
    bool isNonRootInlineBox() const { return m_type == Type::NonRootInlineBox; }
    bool isRootInlineBox() const { return m_type == Type::RootInlineBox; }
    bool isGenericInlineLevelBox() const { return m_type == Type::GenericInlineLevelBox; }
    bool isInlineLevelBox() const { return isAtomicInlineLevelBox() || isLineBreakBox() || isInlineBox() || isGenericInlineLevelBox(); }
    bool isNonRootInlineLevelBox() const { return isInlineLevelBox() && !isRootInlineBox(); }
    Type type() const { return m_type; }

    UBiDiLevel bidiLevel() const { return m_bidiLevel; }

    bool hasContent() const { return m_hasContent; }

    const Layout::InlineRect& logicalRect() const { return m_logicalRect; }
    const Layout::InlineRect& inkOverflow() const { return m_inkOverflow; }

    Layout::InlineLayoutUnit logicalTop() const { return logicalRect().top(); }
    Layout::InlineLayoutUnit logicalBottom() const { return logicalRect().bottom(); }
    Layout::InlineLayoutUnit logicalLeft() const { return logicalRect().left(); }
    Layout::InlineLayoutUnit logicalRight() const { return logicalRect().right(); }

    Layout::InlineLayoutUnit logicalWidth() const { return logicalRect().width(); }
    Layout::InlineLayoutUnit logicalHeight() const { return logicalRect().height(); }

    void moveVertically(Layout::InlineLayoutUnit offset) { m_logicalRect.moveVertically(offset); }
    void adjustInkOverflow(const Layout::InlineRect& childBorderBox) { return m_inkOverflow.expandToContain(childBorderBox); }

    std::optional<Text>& text() { return m_text; }
    const std::optional<Text>& text() const { return m_text; }

    struct Expansion {
        ExpansionBehavior behavior { DefaultExpansion };
        Layout::InlineLayoutUnit horizontalExpansion { 0 };
    };
    Expansion expansion() const { return m_expansion; }

    const Layout::Box& layoutBox() const { return m_layoutBox; }
    const RenderStyle& style() const { return !lineIndex() ? layoutBox().firstLineStyle() : layoutBox().style(); }

    size_t lineIndex() const { return m_lineIndex; }
    // These functions tell you whether this display box is the first/last for the associated inline level box (Layout::Box) and not whether it's the first/last box on the line.
    // (e.g. always true for atomic boxes, but inline boxes spanning over multiple lines can produce individual first/last boxes).
    bool isFirstBox() const { return m_isFirstWithinInlineLevelBox; }
    bool isLastBox() const { return m_isLastWithinInlineLevelBox; }

private:
    const size_t m_lineIndex { 0 };
    const Type m_type { Type::GenericInlineLevelBox };
    CheckedRef<const Layout::Box> m_layoutBox;
    UBiDiLevel m_bidiLevel { UBIDI_DEFAULT_LTR };
    Layout::InlineRect m_logicalRect;
    Layout::InlineRect m_inkOverflow;
    bool m_hasContent : 1;
    bool m_isFirstWithinInlineLevelBox : 1;
    bool m_isLastWithinInlineLevelBox : 1;
    Expansion m_expansion;
    std::optional<Text> m_text;
};

inline Box::Box(size_t lineIndex, Type type, const Layout::Box& layoutBox, UBiDiLevel bidiLevel, const Layout::InlineRect& logicalRect, const Layout::InlineRect& inkOverflow, Expansion expansion, std::optional<Text> text, bool hasContent, OptionSet<PositionWithinInlineLevelBox> positionWithinInlineLevelBox)
    : m_lineIndex(lineIndex)
    , m_type(type)
    , m_layoutBox(layoutBox)
    , m_bidiLevel(bidiLevel)
    , m_logicalRect(logicalRect)
    , m_inkOverflow(inkOverflow)
    , m_hasContent(hasContent)
    , m_isFirstWithinInlineLevelBox(positionWithinInlineLevelBox.contains(PositionWithinInlineLevelBox::First))
    , m_isLastWithinInlineLevelBox(positionWithinInlineLevelBox.contains(PositionWithinInlineLevelBox::Last))
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

}
}
#endif
