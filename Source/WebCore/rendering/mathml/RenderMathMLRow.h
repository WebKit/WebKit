/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
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

#ifndef RenderMathMLRow_h
#define RenderMathMLRow_h

#if ENABLE(MATHML)

#include "RenderMathMLBlock.h"

namespace WebCore {

class RenderMathMLRoot;

class RenderMathMLRow : public RenderMathMLBlock {
public:
    RenderMathMLRow(Element&, Ref<RenderStyle>&&);
    RenderMathMLRow(Document&, Ref<RenderStyle>&&);

    void updateOperatorProperties();

    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) final;
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) final;
    Optional<int> firstLineBaseline() const final;

private:
    bool isRenderMathMLRow() const final { return true; }
    const char* renderName() const override { return isAnonymous() ? "RenderMathMLRow (anonymous)" : "RenderMathMLRow"; }

    void layoutRowItems(int stretchHeightAboveBaseline, int stretchDepthBelowBaseline);
    void computeLineVerticalStretch(int& stretchHeightAboveBaseline, int& stretchDepthBelowBaseline);
    void computePreferredLogicalWidths() override;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLRow, isRenderMathMLRow())

#endif // ENABLE(MATHML)
#endif // RenderMathMLRow_h
