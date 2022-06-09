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

#pragma once

#if ENABLE(MATHML)

#include "MathMLRowElement.h"

namespace WebCore {

class MathMLSelectElement final : public MathMLRowElement {
    WTF_MAKE_ISO_ALLOCATED(MathMLSelectElement);
public:
    static Ref<MathMLSelectElement> create(const QualifiedName& tagName, Document&);
    static bool isMathMLEncoding(const AtomString& value);
    static bool isSVGEncoding(const AtomString& value);
    static bool isHTMLEncoding(const AtomString& value);

private:
    MathMLSelectElement(const QualifiedName& tagName, Document&);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    bool childShouldCreateRenderer(const Node&) const final;

    void finishParsingChildren() final;
    void childrenChanged(const ChildChange&) final;
    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason = ModifiedDirectly) final;
    void defaultEventHandler(Event&) final;
    bool willRespondToMouseClickEvents() final;

    void toggle();
    int getSelectedActionChildAndIndex(Element*& selectedChild);
    Element* getSelectedActionChild();
    Element* getSelectedSemanticsChild();

    void updateSelectedChild() final;
    RefPtr<Element> m_selectedChild;
};

}

#endif // ENABLE(MATHML)
