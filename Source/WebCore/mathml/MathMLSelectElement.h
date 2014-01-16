/*
 * Copyright (C) 2013 The MathJax Consortium. All rights reserved.
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

#ifndef MathMLSelectElement_h
#define MathMLSelectElement_h

#if ENABLE(MATHML)
#include "MathMLInlineContainerElement.h"

namespace WebCore {

class MathMLSelectElement final : public MathMLInlineContainerElement {
public:
    static PassRefPtr<MathMLSelectElement> create(const QualifiedName& tagName, Document&);

private:
    MathMLSelectElement(const QualifiedName& tagName, Document&);
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;

    virtual bool childShouldCreateRenderer(const Node&) const override;

    virtual void finishParsingChildren() override;
    virtual void childrenChanged(const ChildChange&) override;
    virtual void attributeChanged(const QualifiedName&, const AtomicString&, AttributeModificationReason = ModifiedDirectly) override;
    virtual void defaultEventHandler(Event*) override;
    virtual bool willRespondToMouseClickEvents() override;

    void toggle();
    int getSelectedActionChildAndIndex(Element*& selectedChild);
    Element* getSelectedActionChild();
    Element* getSelectedSemanticsChild();

    void updateSelectedChild() override;
    Element* m_selectedChild;
};

}

#endif // ENABLE(MATHML)
#endif // MathMLSelectElement_h
