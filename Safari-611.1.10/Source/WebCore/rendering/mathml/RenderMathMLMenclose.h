/*
 * Copyright (C) 2014 Gurpreet Kaur (k.gurpreet@samsung.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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

#include "MathMLMencloseElement.h"
#include "RenderMathMLRow.h"

namespace WebCore {

class RenderMathMLMenclose final : public RenderMathMLRow {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLMenclose);
public:
    RenderMathMLMenclose(MathMLMencloseElement&, RenderStyle&&);

private:
    const char* renderName() const final { return "RenderMathMLMenclose"; }
    void computePreferredLogicalWidths() final;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;
    void paint(PaintInfo&, const LayoutPoint&) final;

    LayoutUnit ruleThickness() const;
    bool hasNotation(MathMLMencloseElement::MencloseNotationFlag notationFlag) const { return downcast<MathMLMencloseElement>(element()).hasNotation(notationFlag); }

    struct SpaceAroundContent {
        LayoutUnit left;
        LayoutUnit right;
        LayoutUnit top;
        LayoutUnit bottom;
    };
    SpaceAroundContent spaceAroundContent(LayoutUnit contentWidth, LayoutUnit contentHeight) const;

    LayoutRect m_contentRect;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLMenclose, isRenderMathMLMenclose())

#endif // ENABLE(MATHML)
