/*
 * Copyright (C) 2014 Frédéric Wang (fred.wang@free.fr). All rights reserved.
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

#ifndef RenderMathMLToken_h
#define RenderMathMLToken_h

#if ENABLE(MATHML)

#include "MathMLTextElement.h"
#include "RenderMathMLBlock.h"
#include "RenderText.h"

namespace WebCore {
    
class RenderMathMLToken : public RenderMathMLBlock {
public:
    RenderMathMLToken(Element&, PassRef<RenderStyle>);
    RenderMathMLToken(Document&, PassRef<RenderStyle>);

    MathMLTextElement& element() { return static_cast<MathMLTextElement&>(nodeForNonAnonymous()); }

    virtual bool isRenderMathMLToken() const override final { return true; }
    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const override { return true; };
    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild) override;
    virtual void updateTokenContent();
    void updateFromElement() override;

protected:
    void createWrapperIfNeeded();

private:
    virtual const char* renderName() const override { return isAnonymous() ? "RenderMathMLToken (anonymous)" : "RenderMathMLToken"; }
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual void updateStyle();

    // This boolean indicates whether the token element contains some RenderElement descendants, other than the anonymous renderers created for layout purpose.
    bool m_containsElement;
};

RENDER_OBJECT_TYPE_CASTS(RenderMathMLToken, isRenderMathMLToken())

}

#endif // ENABLE(MATHML)
#endif // RenderMathMLToken_h
