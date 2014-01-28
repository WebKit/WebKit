/*
 * Copyright (C) 2014 Gurpreet Kaur (k.gurpreet@samsung.com). All rights reserved.
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

#ifndef MathMLMencloseElement_h
#define MathMLMencloseElement_h

#if ENABLE(MATHML)
#include "MathMLInlineContainerElement.h"

namespace WebCore {

class MathMLMencloseElement final: public MathMLInlineContainerElement {
public:
    static PassRefPtr<MathMLMencloseElement> create(const QualifiedName& tagName, Document&);
    const Vector<String>& notationValues() const { return m_notationValues; }
    bool isRadical() const { return m_isRadicalValue; }

private:
    MathMLMencloseElement(const QualifiedName&, Document&);
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    virtual void finishParsingChildren() override;
    String longDivLeftPadding() const;

    Vector<String> m_notationValues;
    bool m_isRadicalValue;
};

inline MathMLMencloseElement* toMathMLMencloseElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || (node->isElementNode() && toElement(node)->hasTagName(MathMLNames::mencloseTag)));
    return static_cast<MathMLMencloseElement*>(node);
}

}

#endif // ENABLE(MATHML)
#endif // MathMLMencloseElement_h
