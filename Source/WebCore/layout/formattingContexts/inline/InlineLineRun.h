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

namespace WebCore {
namespace Layout {

struct Run {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    struct Text {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
    public:
        Text(size_t position, size_t length, const String& originalContent, String adjustedContentToRender = String(), bool hasHyphen = false);

        size_t start() const { return m_start; }
        size_t end() const { return start() + length(); }
        size_t length() const { return m_length; }
        String originalContent() const { return m_originalContent; }
        String renderedContent() const { return m_adjustedContentToRender; }

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
    Run(size_t lineIndex, Type, const Box&, const InlineRect&, const InlineRect& inkOverflow, Expansion, std::optional<Text> = std::nullopt, bool hasContent = true, bool isLineSpanning = false);

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

    bool hasContent() const { return m_hasContent; }
    bool isLineSpanning() const { return m_isLineSpanning; }

    const InlineRect& logicalRect() const { return m_logicalRect; }
    const InlineRect& inkOverflow() const { return m_inkOverflow; }

    InlineLayoutUnit logicalTop() const { return logicalRect().top(); }
    InlineLayoutUnit logicalBottom() const { return logicalRect().bottom(); }
    InlineLayoutUnit logicalLeft() const { return logicalRect().left(); }
    InlineLayoutUnit logicalRight() const { return logicalRect().right(); }

    InlineLayoutUnit logicalWidth() const { return logicalRect().width(); }
    InlineLayoutUnit logicalHeight() const { return logicalRect().height(); }

    void moveVertically(InlineLayoutUnit offset) { m_logicalRect.moveVertically(offset); }
    std::optional<Text>& text() { return m_text; }
    const std::optional<Text>& text() const { return m_text; }

    struct Expansion {
        ExpansionBehavior behavior { DefaultExpansion };
        InlineLayoutUnit horizontalExpansion { 0 };
    };
    Expansion expansion() const { return m_expansion; }

    const Box& layoutBox() const { return *m_layoutBox; }
    const RenderStyle& style() const { return layoutBox().style(); }

    size_t lineIndex() const { return m_lineIndex; }

private:
    const size_t m_lineIndex;
    const Type m_type;
    WeakPtr<const Layout::Box> m_layoutBox;
    InlineRect m_logicalRect;
    InlineRect m_inkOverflow;
    bool m_hasContent { true };
    // FIXME: This is temporary until after iterators can skip over line spanning/root inline boxes.
    bool m_isLineSpanning { false };
    Expansion m_expansion;
    std::optional<Text> m_text;
};

inline Run::Run(size_t lineIndex, Type type, const Layout::Box& layoutBox, const InlineRect& logicalRect, const InlineRect& inkOverflow, Expansion expansion, std::optional<Text> text, bool hasContent, bool isLineSpanning)
    : m_lineIndex(lineIndex)
    , m_type(type)
    , m_layoutBox(makeWeakPtr(layoutBox))
    , m_logicalRect(logicalRect)
    , m_inkOverflow(inkOverflow)
    , m_hasContent(hasContent)
    , m_isLineSpanning(isLineSpanning)
    , m_expansion(expansion)
    , m_text(text)
{
}

inline Run::Text::Text(size_t start, size_t length, const String& originalContent, String adjustedContentToRender, bool hasHyphen)
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
