/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
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

#ifndef RenderMathMLRoot_h
#define RenderMathMLRoot_h

#if ENABLE(MATHML)

#include "RenderMathMLBlock.h"

namespace WebCore {
    
// Render base^(1/index), or sqrt(base) via the derived class RenderMathMLSquareRoot, using radical notation.
class RenderMathMLRoot : public RenderMathMLBlock {
public:
    RenderMathMLRoot(Element&, PassRef<RenderStyle>);
    RenderMathMLRoot(Document&, PassRef<RenderStyle>);

    virtual LayoutUnit paddingTop() const override;
    virtual LayoutUnit paddingBottom() const override;
    virtual LayoutUnit paddingLeft() const override;
    virtual LayoutUnit paddingRight() const override;
    virtual LayoutUnit paddingBefore() const override;
    virtual LayoutUnit paddingAfter() const override;
    virtual LayoutUnit paddingStart() const override;
    virtual LayoutUnit paddingEnd() const override;

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) override;
    
protected:
    virtual void layout() override;
    
    virtual void paint(PaintInfo&, const LayoutPoint&) override;

private:
    virtual bool isRenderMathMLRoot() const override final { return true; }
    virtual const char* renderName() const override { return "RenderMathMLRoot"; }
    
    // This may return 0 for a non-MathML index (which won't occur in valid MathML).
    RenderBox* index() const;

    int m_intrinsicPaddingBefore;
    int m_intrinsicPaddingAfter;
    int m_intrinsicPaddingStart;
    int m_intrinsicPaddingEnd;
    int m_overbarLeftPointShift;
    int m_indexTop;
};
    
}

#endif // ENABLE(MATHML)

#endif // RenderMathMLRoot_h
