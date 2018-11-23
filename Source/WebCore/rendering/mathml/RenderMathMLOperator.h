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

#include "MathMLOperatorDictionary.h"
#include "MathOperator.h"
#include "RenderMathMLToken.h"

namespace WebCore {

class MathMLOperatorElement;

class RenderMathMLOperator : public RenderMathMLToken {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLOperator);
public:
    RenderMathMLOperator(MathMLOperatorElement&, RenderStyle&&);
    RenderMathMLOperator(Document&, RenderStyle&&);
    MathMLOperatorElement& element() const;

    void stretchTo(LayoutUnit heightAboveBaseline, LayoutUnit depthBelowBaseline);
    void stretchTo(LayoutUnit width);
    LayoutUnit stretchSize() const { return isVertical() ? m_stretchHeightAboveBaseline + m_stretchDepthBelowBaseline : m_stretchWidth; }
    void resetStretchSize();
    void setStretchWidthLocked(bool stretchWidthLocked) { m_isStretchWidthLocked = stretchWidthLocked; }
    bool isStretchWidthLocked() const { return m_isStretchWidthLocked; }

    virtual bool hasOperatorFlag(MathMLOperatorDictionary::Flag) const;
    bool isLargeOperatorInDisplayStyle() const { return !hasOperatorFlag(MathMLOperatorDictionary::Stretchy) && hasOperatorFlag(MathMLOperatorDictionary::LargeOp) && mathMLStyle().displayStyle(); }
    bool shouldMoveLimits() const { return hasOperatorFlag(MathMLOperatorDictionary::MovableLimits) && !mathMLStyle().displayStyle(); }
    virtual bool isVertical() const;
    LayoutUnit italicCorrection() const { return m_mathOperator.italicCorrection(); }

    void updateTokenContent() final;
    void updateFromElement() final;
    virtual UChar32 textContent() const;
    bool isStretchy() const { return textContent() && hasOperatorFlag(MathMLOperatorDictionary::Stretchy); }

protected:
    virtual void updateMathOperator();
    virtual LayoutUnit leadingSpace() const;
    virtual LayoutUnit trailingSpace() const;
    virtual LayoutUnit minSize() const;
    virtual LayoutUnit maxSize() const;
    virtual bool useMathOperator() const;

private:
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;
    void computePreferredLogicalWidths() final;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;
    void paint(PaintInfo&, const LayoutPoint&) final;

    const char* renderName() const final { return isAnonymous() ? "RenderMathMLOperator (anonymous)" : "RenderMathMLOperator"; }
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) final;
    bool isRenderMathMLOperator() const final { return true; }
    bool isInvisibleOperator() const;

    std::optional<int> firstLineBaseline() const final;
    RenderMathMLOperator* unembellishedOperator() const final { return const_cast<RenderMathMLOperator*>(this); }

    LayoutUnit verticalStretchedOperatorShift() const;

    LayoutUnit m_stretchHeightAboveBaseline { 0 };
    LayoutUnit m_stretchDepthBelowBaseline { 0 };
    LayoutUnit m_stretchWidth;
    bool m_isStretchWidthLocked { false };

    MathOperator m_mathOperator;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLOperator, isRenderMathMLOperator())

#endif // ENABLE(MATHML)
