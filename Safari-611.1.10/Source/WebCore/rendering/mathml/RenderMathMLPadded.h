/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
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

#include "MathMLPaddedElement.h"
#include "RenderMathMLRow.h"

namespace WebCore {

class RenderMathMLPadded final : public RenderMathMLRow {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLPadded);
public:
    RenderMathMLPadded(MathMLPaddedElement&, RenderStyle&&);

private:
    const char* renderName() const final { return "RenderMathMLPadded"; }
    bool isRenderMathMLPadded() const final { return true; }

    void computePreferredLogicalWidths() final;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;
    Optional<int> firstLineBaseline() const final;

    MathMLPaddedElement& element() const { return static_cast<MathMLPaddedElement&>(nodeForNonAnonymous()); }
    LayoutUnit voffset() const;
    LayoutUnit lspace() const;
    LayoutUnit mpaddedWidth(LayoutUnit contentWidth) const;
    LayoutUnit mpaddedHeight(LayoutUnit contentHeight) const;
    LayoutUnit mpaddedDepth(LayoutUnit contentDepth) const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLPadded, isRenderMathMLPadded())

#endif // ENABLE(MATHML)
