/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef PseudoElement_h
#define PseudoElement_h

#include "Element.h"
#include "RenderStyleConstants.h"
#include <wtf/Forward.h>

namespace WebCore {

class ContentData;

class PseudoElement : public Element {
public:
    static PassRefPtr<PseudoElement> create(Element* parent, PseudoId pseudoId)
    {
        return adoptRef(new PseudoElement(parent, pseudoId));
    }

    static bool pseudoRendererIsNeeded(RenderStyle*);

    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE;
    virtual void attach() OVERRIDE;
    virtual bool rendererIsNeeded(const NodeRenderingContext&) OVERRIDE;
    virtual PseudoId virtualPseudoId() const OVERRIDE { return m_pseudoId; }

private:
    PseudoElement(Element*, PseudoId);
    virtual void didRecalcStyle(StyleChange) OVERRIDE;
    void updateChildStyle(RenderObject*) const;
    RenderObject* createRendererForContent(const ContentData*) const;

    PseudoId m_pseudoId;
};

inline PseudoElement* toPseudoElement(Node* node)
{
    ASSERT(!node || node->isPseudoElement());
    return static_cast<PseudoElement*>(node);
}

inline const PseudoElement* toPseudoElement(const Node* node)
{
    ASSERT(!node || node->isPseudoElement());
    return static_cast<const PseudoElement*>(node);
}

// This will catch anyone doing an unnecessary cast.
void toPseudoElement(const Element*);

const QualifiedName& pseudoElementName();

} // namespace

#endif
