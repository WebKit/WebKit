/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2012 David Barton (dbarton@mathscribe.com). All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MATHML)

#include "MathMLElement.h"
#include "MathMLStyle.h"
#include "RenderBlock.h"
#include "RenderTable.h"

namespace WebCore {

class RenderMathMLOperator;
class MathMLPresentationElement;

class RenderMathMLBlock : public RenderBlock {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLBlock);
public:
    RenderMathMLBlock(Type, MathMLPresentationElement&, RenderStyle&&);
    RenderMathMLBlock(Type, Document&, RenderStyle&&);
    virtual ~RenderMathMLBlock();

    MathMLStyle& mathMLStyle() const { return m_mathMLStyle; }

    bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;

    // MathML defines an "embellished operator" as roughly an <mo> that may have subscripts,
    // superscripts, underscripts, overscripts, or a denominator (as in d/dx, where "d" is some
    // differential operator). The padding, precedence, and stretchiness of the base <mo> should
    // apply to the embellished operator as a whole. unembellishedOperator() checks for being an
    // embellished operator, and omits any embellishments.
    // FIXME: We don't yet handle all the cases in the MathML spec. See
    // https://bugs.webkit.org/show_bug.cgi?id=78617.
    virtual RenderMathMLOperator* unembellishedOperator() const { return 0; }

    LayoutUnit baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;

#if ENABLE(DEBUG_MATH_LAYOUT)
    virtual void paint(PaintInfo&, const LayoutPoint&);
#endif

protected:
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    inline LayoutUnit ruleThicknessFallback() const;

    LayoutUnit mathAxisHeight() const;
    LayoutUnit mirrorIfNeeded(LayoutUnit horizontalOffset, LayoutUnit boxWidth = 0_lu) const;
    inline LayoutUnit mirrorIfNeeded(LayoutUnit horizontalOffset, const RenderBox& child) const;

    static inline LayoutUnit ascentForChild(const RenderBox& child);

    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) override;
    void layoutInvalidMarkup(bool relayoutChildren);

private:
    bool isRenderMathMLBlock() const final { return true; }
    ASCIILiteral renderName() const override { return "RenderMathMLBlock"_s; }
    bool avoidsFloats() const final { return true; }
    bool canDropAnonymousBlockChild() const final { return false; }
    void layoutItems(bool relayoutChildren);

    Ref<MathMLStyle> m_mathMLStyle;
};

class RenderMathMLTable final : public RenderTable {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLTable);
public:
    inline RenderMathMLTable(MathMLElement&, RenderStyle&&);

    MathMLStyle& mathMLStyle() const { return m_mathMLStyle; }

private:
    ASCIILiteral renderName() const final { return "RenderMathMLTable"_s; }
    std::optional<LayoutUnit> firstLineBaseline() const final;

    Ref<MathMLStyle> m_mathMLStyle;
};

LayoutUnit toUserUnits(const MathMLElement::Length&, const RenderStyle&, const LayoutUnit& referenceValue);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLBlock, isRenderMathMLBlock())
SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLTable, isRenderMathMLTable())

#endif // ENABLE(MATHML)
