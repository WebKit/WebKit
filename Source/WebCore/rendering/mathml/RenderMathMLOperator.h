/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
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
#include "MathMLOperatorDictionary.h"
#include "MathOperator.h"
#include "RenderMathMLToken.h"

namespace WebCore {

class MathMLOperatorElement;

class RenderMathMLOperator : public RenderMathMLToken {
public:
    RenderMathMLOperator(MathMLOperatorElement&, RenderStyle&&);
    RenderMathMLOperator(Document&, RenderStyle&&);
    MathMLOperatorElement& element() const;

    void stretchTo(LayoutUnit heightAboveBaseline, LayoutUnit depthBelowBaseline);
    void stretchTo(LayoutUnit width);
    LayoutUnit stretchSize() const { return m_isVertical ? m_stretchHeightAboveBaseline + m_stretchDepthBelowBaseline : m_stretchWidth; }
    void resetStretchSize();

    virtual bool hasOperatorFlag(MathMLOperatorDictionary::Flag) const;
    bool isLargeOperatorInDisplayStyle() const { return !hasOperatorFlag(MathMLOperatorDictionary::Stretchy) && hasOperatorFlag(MathMLOperatorDictionary::LargeOp) && mathMLStyle()->displayStyle(); }
    bool shouldMoveLimits() const { return hasOperatorFlag(MathMLOperatorDictionary::MovableLimits) && !mathMLStyle()->displayStyle(); }
    bool isVertical() const { return m_isVertical; }
    LayoutUnit italicCorrection() const { return m_mathOperator.italicCorrection(); }

    void updateTokenContent() final;
    void updateOperatorProperties();
    void updateFromElement() final;
    virtual UChar textContent() const;

protected:
    void rebuildTokenContent();
    virtual void setOperatorProperties();

    bool m_isVertical { true };
    LayoutUnit m_leadingSpace;
    LayoutUnit m_trailingSpace;
    LayoutUnit m_minSize;
    LayoutUnit m_maxSize;

private:
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;
    void computePreferredLogicalWidths() final;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) final;
    void paint(PaintInfo&, const LayoutPoint&) final;

    void setLeadingSpace(LayoutUnit leadingSpace) { m_leadingSpace = leadingSpace; }
    void setTrailingSpace(LayoutUnit trailingSpace) { m_trailingSpace = trailingSpace; }

    const char* renderName() const final { return isAnonymous() ? "RenderMathMLOperator (anonymous)" : "RenderMathMLOperator"; }
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) final;
    bool isRenderMathMLOperator() const final { return true; }
    // The following operators are invisible: U+2061 FUNCTION APPLICATION, U+2062 INVISIBLE TIMES, U+2063 INVISIBLE SEPARATOR, U+2064 INVISIBLE PLUS.
    bool isInvisibleOperator() { return 0x2061 <= textContent() && textContent() <= 0x2064; }

    Optional<int> firstLineBaseline() const final;
    RenderMathMLOperator* unembellishedOperator() final { return this; }

    bool shouldAllowStretching() const;
    bool useMathOperator() const;

    LayoutUnit verticalStretchedOperatorShift() const;

    LayoutUnit m_stretchHeightAboveBaseline { 0 };
    LayoutUnit m_stretchDepthBelowBaseline { 0 };
    LayoutUnit m_stretchWidth;

    MathOperator m_mathOperator;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLOperator, isRenderMathMLOperator())

#endif // ENABLE(MATHML)
