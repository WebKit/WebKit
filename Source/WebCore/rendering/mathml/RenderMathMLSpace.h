/*
 * Copyright (C) 2013 The MathJax Consortium. All rights reserved.
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

#ifndef RenderMathMLSpace_h
#define RenderMathMLSpace_h

#if ENABLE(MATHML)

#include "MathMLTextElement.h"
#include "RenderMathMLBlock.h"

namespace WebCore {
    
class RenderMathMLSpace final : public RenderMathMLBlock {
public:
    RenderMathMLSpace(MathMLTextElement&, PassRef<RenderStyle>);
    MathMLTextElement& element() { return static_cast<MathMLTextElement&>(nodeForNonAnonymous()); }

private:
    virtual const char* renderName() const override { return isAnonymous() ? "RenderMathMLSpace (anonymous)" : "RenderMathMLSpace"; }
    virtual bool isRenderMathMLSpace() const override { return true; }
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const override { return false; }
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void updateFromElement() override;
    virtual int firstLineBaseline() const override;
    virtual void updateLogicalWidth() override;
    virtual void updateLogicalHeight() override;

    LayoutUnit m_width;
    LayoutUnit m_height;
    LayoutUnit m_depth;
};

RENDER_OBJECT_TYPE_CASTS(RenderMathMLSpace, isRenderMathMLSpace())

}

#endif // ENABLE(MATHML)
#endif // RenderMathMLSpace_h
