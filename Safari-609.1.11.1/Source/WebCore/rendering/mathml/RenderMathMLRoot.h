/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
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

#include "MathOperator.h"
#include "RenderMathMLRow.h"

namespace WebCore {

class MathMLRootElement;

enum class RootType { SquareRoot, RootWithIndex };

// Render base^(1/index), or sqrt(base) using radical notation.
class RenderMathMLRoot final : public RenderMathMLRow {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLRoot);
public:
    RenderMathMLRoot(MathMLRootElement&, RenderStyle&&);
    void updateStyle();

private:
    bool isValid() const;
    RenderBox& getBase() const;
    RenderBox& getIndex() const;
    bool isRenderMathMLRoot() const final { return true; }
    const char* renderName() const final { return "RenderMathMLRoot"; }
    MathMLRootElement& element() const;
    RootType rootType() const;

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;

    void computePreferredLogicalWidths() final;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;
    void paint(PaintInfo&, const LayoutPoint&) final;

    struct HorizontalParameters {
        LayoutUnit kernBeforeDegree;
        LayoutUnit kernAfterDegree;
    };
    HorizontalParameters horizontalParameters();
    struct VerticalParameters {
        LayoutUnit verticalGap;
        LayoutUnit ruleThickness;
        LayoutUnit extraAscender;
        float degreeBottomRaisePercent;
    };
    VerticalParameters verticalParameters();

    MathOperator m_radicalOperator;
    LayoutUnit m_radicalOperatorTop;
    LayoutUnit m_baseWidth;

    bool isRenderMathMLSquareRoot() const final { return rootType() == RootType::SquareRoot; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLRoot, isRenderMathMLRoot())

#endif // ENABLE(MATHML)
