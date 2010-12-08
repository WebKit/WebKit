/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShadowElement_h
#define ShadowElement_h

#include "HTMLDivElement.h"
#include "HTMLInputElement.h"

namespace WebCore {

template<class BaseElement>
class ShadowElement : public BaseElement {
protected:
    ShadowElement(const QualifiedName& name, HTMLElement* shadowParent)
        : BaseElement(name, shadowParent->document())
        , m_shadowParent(shadowParent)
    {
    }

    HTMLElement* shadowParent() const { return m_shadowParent.get(); }

private:
    virtual bool isShadowNode() const { return true; }
    virtual ContainerNode* shadowParentNode() { return m_shadowParent.get(); }

    RefPtr<HTMLElement> m_shadowParent;
};

class ShadowBlockElement : public ShadowElement<HTMLDivElement> {
public:
    static PassRefPtr<ShadowBlockElement> create(HTMLElement*);
    static PassRefPtr<ShadowBlockElement> createForPart(HTMLElement*, PseudoId);
    static bool partShouldHaveStyle(const RenderObject* parentRenderer, PseudoId pseudoId);
    void layoutAsPart(const IntRect& partRect);
    virtual void updateStyleForPart(PseudoId);

protected:
    ShadowBlockElement(HTMLElement*);
    void initAsPart(PseudoId pasuedId);
private:
    static PassRefPtr<RenderStyle> createStyleForPart(RenderObject*, PseudoId);
};

class ShadowInputElement : public ShadowElement<HTMLInputElement> {
public:
    static PassRefPtr<ShadowInputElement> create(HTMLElement*);
protected:
    ShadowInputElement(HTMLElement*);
};

} // namespace WebCore

#endif // ShadowElement_h
