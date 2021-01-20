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

#pragma once

#if ENABLE(MATHML)

#include "MathMLSpaceElement.h"
#include "RenderMathMLBlock.h"

namespace WebCore {

class RenderMathMLSpace final : public RenderMathMLBlock {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLSpace);
public:
    RenderMathMLSpace(MathMLSpaceElement&, RenderStyle&&);
    MathMLSpaceElement& element() const { return static_cast<MathMLSpaceElement&>(nodeForNonAnonymous()); }

private:
    const char* renderName() const final { return "RenderMathMLSpace"; }
    bool isRenderMathMLSpace() const final { return true; }
    bool isChildAllowed(const RenderObject&, const RenderStyle&) const final { return false; }
    void computePreferredLogicalWidths() final;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;
    Optional<int> firstLineBaseline() const final;

    LayoutUnit spaceWidth() const;
    void getSpaceHeightAndDepth(LayoutUnit& height, LayoutUnit& depth) const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLSpace, isRenderMathMLSpace())

#endif // ENABLE(MATHML)
