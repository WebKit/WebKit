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

#include "RenderMathMLScripts.h"

namespace WebCore {

class MathMLUnderOverElement;

class RenderMathMLUnderOver final : public RenderMathMLScripts {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLUnderOver);
public:
    RenderMathMLUnderOver(MathMLUnderOverElement&, RenderStyle&&);

private:
    bool isRenderMathMLScripts() const final { return false; }
    bool isRenderMathMLUnderOver() const final { return true; }
    const char* renderName() const final { return "RenderMathMLUnderOver"; }
    MathMLUnderOverElement& element() const;

    void computePreferredLogicalWidths() final;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;

    void stretchHorizontalOperatorsAndLayoutChildren();
    bool isValid() const;
    bool shouldMoveLimits() const;
    RenderBox& base() const;
    RenderBox& under() const;
    RenderBox& over() const;
    LayoutUnit horizontalOffset(const RenderBox&) const;
    bool hasAccent(bool accentUnder = false) const;
    bool hasAccentUnder() const { return hasAccent(true); };
    struct VerticalParameters {
        bool useUnderOverBarFallBack;
        LayoutUnit underGapMin;
        LayoutUnit overGapMin;
        LayoutUnit underShiftMin;
        LayoutUnit overShiftMin;
        LayoutUnit underExtraDescender;
        LayoutUnit overExtraAscender;
        LayoutUnit accentBaseHeight;
    };
    VerticalParameters verticalParameters() const;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLUnderOver, isRenderMathMLUnderOver())

#endif // ENABLE(MATHML)
