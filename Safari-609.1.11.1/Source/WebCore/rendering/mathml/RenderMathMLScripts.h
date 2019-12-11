/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2013 The MathJax Consortium.
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

#include "MathMLScriptsElement.h"
#include "RenderMathMLBlock.h"

namespace WebCore {

// Render a base with scripts.
class RenderMathMLScripts : public RenderMathMLBlock {
    WTF_MAKE_ISO_ALLOCATED(RenderMathMLScripts);
public:
    RenderMathMLScripts(MathMLScriptsElement&, RenderStyle&&);
    RenderMathMLOperator* unembellishedOperator() const final;

protected:
    bool isRenderMathMLScripts() const override { return true; }
    const char* renderName() const override { return "RenderMathMLScripts"; }
    MathMLScriptsElement::ScriptType scriptType() const;
    void computePreferredLogicalWidths() override;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) override;

private:
    MathMLScriptsElement& element() const;
    Optional<int> firstLineBaseline() const final;
    struct ReferenceChildren {
        RenderBox* base;
        RenderBox* prescriptDelimiter;
        RenderBox* firstPostScript;
        RenderBox* firstPreScript;
    };
    Optional<ReferenceChildren> validateAndGetReferenceChildren();
    LayoutUnit spaceAfterScript();
    LayoutUnit italicCorrection(const ReferenceChildren&);
    struct VerticalParameters {
        LayoutUnit subscriptShiftDown;
        LayoutUnit superscriptShiftUp;
        LayoutUnit subscriptBaselineDropMin;
        LayoutUnit superScriptBaselineDropMax;
        LayoutUnit subSuperscriptGapMin;
        LayoutUnit superscriptBottomMin;
        LayoutUnit subscriptTopMax;
        LayoutUnit superscriptBottomMaxWithSubscript;
    };
    VerticalParameters verticalParameters() const;
    struct VerticalMetrics {
        LayoutUnit subShift;
        LayoutUnit supShift;
        LayoutUnit ascent;
        LayoutUnit descent;
    };
    VerticalMetrics verticalMetrics(const ReferenceChildren&);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLScripts, isRenderMathMLScripts())

#endif // ENABLE(MATHML)
