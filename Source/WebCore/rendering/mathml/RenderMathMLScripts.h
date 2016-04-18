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

#ifndef RenderMathMLScripts_h
#define RenderMathMLScripts_h

#if ENABLE(MATHML)

#include "RenderMathMLBlock.h"

namespace WebCore {
// Render a base with scripts.
class RenderMathMLScripts final : public RenderMathMLBlock {
public:
    RenderMathMLScripts(Element&, Ref<RenderStyle>&&);
    RenderMathMLOperator* unembellishedOperator() final;
    Optional<int> firstLineBaseline() const final;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) final;
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) final;

private:
    bool isRenderMathMLScripts() const final { return true; }
    const char* renderName() const final { return "RenderMathMLScripts"; }

    bool getBaseAndScripts(RenderBox*& base, RenderBox*& firstPostScript, RenderBox*& firstPreScript);
    LayoutUnit spaceAfterScript();
    LayoutUnit italicCorrection(RenderBox* base);
    void computePreferredLogicalWidths() override;
    void getScriptMetricsAndLayoutIfNeeded(RenderBox* base, RenderBox* script, LayoutUnit& minSubScriptShift, LayoutUnit& minSupScriptShift, LayoutUnit& maxScriptDescent, LayoutUnit& maxScriptAscent);
    LayoutUnit mirrorIfNeeded(LayoutUnit horizontalOffset, const RenderBox& child);

    enum ScriptsType { Sub, Super, SubSup, Multiscripts };

    ScriptsType m_scriptType;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLScripts, isRenderMathMLScripts())

#endif // ENABLE(MATHML)

#endif // RenderMathMLScripts_h
