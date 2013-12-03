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

#include "config.h"
#include "MathMLSelectElement.h"

#if ENABLE(MATHML)

#include "MathMLNames.h"
#include "RenderMathMLRow.h"

namespace WebCore {

using namespace MathMLNames;

MathMLSelectElement::MathMLSelectElement(const QualifiedName& tagName, Document& document)
    : MathMLInlineContainerElement(tagName, document)
    , m_selectedChild(nullptr)
{
}

PassRefPtr<MathMLSelectElement> MathMLSelectElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new MathMLSelectElement(tagName, document));
}

RenderElement* MathMLSelectElement::createRenderer(PassRef<RenderStyle> style)
{
    return new RenderMathMLRow(*this, std::move(style));
}

bool MathMLSelectElement::childShouldCreateRenderer(const Node& child) const
{
    return MathMLElement::childShouldCreateRenderer(child) && m_selectedChild == &child;
}

void MathMLSelectElement::finishParsingChildren()
{
    updateSelectedChild();
    MathMLInlineContainerElement::finishParsingChildren();
}

void MathMLSelectElement::childrenChanged(const ChildChange& change)
{
    updateSelectedChild();
    MathMLInlineContainerElement::childrenChanged(change);
}

void MathMLSelectElement::attributeChanged(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason reason)
{
    if (hasLocalName(mactionTag) && (name == MathMLNames::actiontypeAttr || name == MathMLNames::selectionAttr))
        updateSelectedChild();

    MathMLInlineContainerElement::attributeChanged(name, newValue, reason);
}

void MathMLSelectElement::updateSelectedChild()
{
    Element* newSelectedChild = firstElementChild();

    if (newSelectedChild) {
        if (hasLocalName(mactionTag)) {
            // The value of the actiontype attribute is case-sensitive.
            const AtomicString& actiontype = fastGetAttribute(MathMLNames::actiontypeAttr);
            if (actiontype == "statusline")
                // FIXME: implement user interaction for the "statusline" action type (http://wkbug/124922).
                { }
            else if (actiontype == "tooltip")
                // FIXME: implement user interaction for the "tooltip" action type (http://wkbug/124921).
                { }
            else {
                // FIXME: implement user interaction for the "toggle" action type (http://wkbug/120059).
                // For the "toggle" action type or any unknown action type, we rely on the value of the selection attribute to determine the visible child.
                int selection = fastGetAttribute(MathMLNames::selectionAttr).toInt();
                for (int i = 1; i < selection; i++) {
                    Element* nextChild = newSelectedChild->nextElementSibling();
                    if (!nextChild)
                        break;
                    newSelectedChild = nextChild;
                }
            }
        } else {
            ASSERT(hasLocalName(semanticsTag));
            // FIXME: implement Gecko's selection algorithm for <semantics> (http://wkbug/100626).
        }
    }

    if (m_selectedChild == newSelectedChild)
        return;

    if (m_selectedChild && m_selectedChild->renderer())
        Style::detachRenderTree(*m_selectedChild);

    m_selectedChild = newSelectedChild;
    setNeedsStyleRecalc();
}

}

#endif // ENABLE(MATHML)
