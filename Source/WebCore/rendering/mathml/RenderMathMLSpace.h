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
    RenderMathMLSpace(MathMLTextElement&, Ref<RenderStyle>&&);
    MathMLTextElement& element() { return static_cast<MathMLTextElement&>(nodeForNonAnonymous()); }

private:
    const char* renderName() const final { return isAnonymous() ? "RenderMathMLSpace (anonymous)" : "RenderMathMLSpace"; }
    bool isRenderMathMLSpace() const final { return true; }
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;
    bool isChildAllowed(const RenderObject&, const RenderStyle&) const final { return false; }
    void updateFromElement() final;
    void computePreferredLogicalWidths() final;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) final;
    Optional<int> firstLineBaseline() const final;

    LayoutUnit m_width;
    LayoutUnit m_height;
    LayoutUnit m_depth;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLSpace, isRenderMathMLSpace())

#endif // ENABLE(MATHML)
#endif // RenderMathMLSpace_h
