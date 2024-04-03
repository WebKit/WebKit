/**
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "InlineLevelBox.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

inline InlineLevelBox::InlineLevelBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutSize logicalSize, Type type, OptionSet<PositionWithinLayoutBox> positionWithinLayoutBox)
    : m_layoutBox(layoutBox)
    , m_logicalRect({ }, logicalLeft, logicalSize.width(), logicalSize.height())
    , m_hasContent(layoutBox.isRubyBase() && layoutBox.associatedRubyAnnotationBox()) // Normally we set inline box's has-content state as we come across child content, but ruby annotations are not visible to inline layout.
    , m_isFirstWithinLayoutBox(positionWithinLayoutBox.contains(PositionWithinLayoutBox::First))
    , m_isLastWithinLayoutBox(positionWithinLayoutBox.contains(PositionWithinLayoutBox::Last))
    , m_type(type)
    , m_style({ style.fontCascade().metricsOfPrimaryFont(), style.lineHeight(), style.textBoxEdge(), style.textBoxTrim(), style.lineBoxContain(), InlineLayoutUnit(style.fontCascade().fontDescription().computedSize()), { } })
{
    m_style.verticalAlignment.type = style.verticalAlign();
    if (m_style.verticalAlignment.type == VerticalAlign::Length)
        m_style.verticalAlignment.baselineOffset = floatValueForLength(style.verticalAlignLength(), preferredLineHeight());
}

inline InlineLevelBox InlineLevelBox::createAtomicInlineLevelBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
{
    return { layoutBox, style, logicalLeft, { logicalWidth, { } }, Type::AtomicInlineLevelBox };
}

inline InlineLevelBox InlineLevelBox::createGenericInlineLevelBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft)
{
    return { layoutBox, style, logicalLeft, { }, Type::GenericInlineLevelBox };
}

inline InlineLevelBox InlineLevelBox::createInlineBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, LineSpanningInlineBox isLineSpanning)
{
    return { layoutBox, style, logicalLeft, { logicalWidth, { } }, isLineSpanning == LineSpanningInlineBox::Yes ? Type::LineSpanningInlineBox : Type::InlineBox, { } };
}

inline InlineLevelBox InlineLevelBox::createLineBreakBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft)
{
    return { layoutBox, style, logicalLeft, { }, Type::LineBreakBox };
}

inline InlineLevelBox InlineLevelBox::createRootInlineBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
{
    return { layoutBox, style, logicalLeft, { logicalWidth, { } }, Type::RootInlineBox, { } };
}

inline bool InlineLevelBox::mayStretchLineBox() const
{
    if (isRootInlineBox())
        return m_style.lineBoxContain.containsAny({ LineBoxContain::Block, LineBoxContain::Inline }) || (hasContent() && m_style.lineBoxContain.containsAny({ LineBoxContain::InitialLetter, LineBoxContain::Font, LineBoxContain::Glyphs }));

    if (isAtomicInlineLevelBox())
        return m_style.lineBoxContain.contains(LineBoxContain::Replaced);

    if (isInlineBox()) {
        // Either the inline box itself is included or its text content thorugh Glyph and Font.
        return m_style.lineBoxContain.containsAny({ LineBoxContain::Inline, LineBoxContain::InlineBox }) || (hasContent() && m_style.lineBoxContain.containsAny({ LineBoxContain::Font, LineBoxContain::Glyphs }));
    }

    if (isLineBreakBox())
        return m_style.lineBoxContain.containsAny({ LineBoxContain::Inline, LineBoxContain::InlineBox }) || (hasContent() && m_style.lineBoxContain.containsAny({ LineBoxContain::Font, LineBoxContain::Glyphs }));

    return true;
}

inline void InlineLevelBox::setTextEmphasis(std::pair<InlineLayoutUnit, InlineLayoutUnit> textEmphasis)
{
    if (!textEmphasis.first && !textEmphasis.second)
        return;
    if (textEmphasis.first) {
        m_textEmphasis = TextEmphasis { textEmphasis.first, 0.f };
        return;
    }
    m_textEmphasis = TextEmphasis { 0.f, textEmphasis.second };
}

}
}

