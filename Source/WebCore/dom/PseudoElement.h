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
#include "Event.h"
#include "RenderStyle.h"
#include <wtf/Forward.h>

namespace WebCore {

class PseudoElement final : public Element {
public:
    static PassRefPtr<PseudoElement> create(Element& host, PseudoId pseudoId)
    {
        return adoptRef(new PseudoElement(host, pseudoId));
    }
    virtual ~PseudoElement();

    Element* hostElement() const { return m_hostElement; }
    void clearHostElement() { m_hostElement = nullptr; }

    virtual PassRefPtr<RenderStyle> customStyleForRenderer(RenderStyle& parentStyle) override;
    virtual void didAttachRenderers() override;
    virtual bool rendererIsNeeded(const RenderStyle&) override;

    // As per http://dev.w3.org/csswg/css3-regions/#flow-into, pseudo-elements such as ::first-line, ::first-letter, ::before or ::after
    // cannot be directly collected into a named flow.
#if ENABLE(CSS_REGIONS)
    virtual bool shouldMoveToFlowThread(const RenderStyle&) const override { return false; }
#endif

    virtual bool canStartSelection() const override { return false; }
    virtual bool canContainRangeEndPoint() const override { return false; }

    static String pseudoElementNameForEvents(PseudoId);

private:
    PseudoElement(Element&, PseudoId);

    virtual void didRecalcStyle(Style::Change) override;
    virtual PseudoId customPseudoId() const override { return m_pseudoId; }

    Element* m_hostElement;
    PseudoId m_pseudoId;
};

const QualifiedName& pseudoElementTagName();

inline bool pseudoElementRendererIsNeeded(const RenderStyle* style)
{
    return style && style->display() != NONE && (style->contentData() || style->hasFlowFrom());
}

void isPseudoElement(const PseudoElement&); // Catch unnecessary runtime check of type known at compile time.
inline bool isPseudoElement(const Node& node) { return node.isPseudoElement(); }
NODE_TYPE_CASTS(PseudoElement)

} // namespace

#endif
