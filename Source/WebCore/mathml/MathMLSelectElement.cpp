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

#include "Event.h"
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

int MathMLSelectElement::getSelectedChildAndIndex(Element*& selectedChild)
{
    // We "round up or down to the closest allowable value" of the selection attribute, as suggested by the MathML specification.
    selectedChild = firstElementChild();
    if (!selectedChild)
        return 1;

    int selection = fastGetAttribute(MathMLNames::selectionAttr).toInt();
    int i;
    for (i = 1; i < selection; i++) {
        Element* nextChild = selectedChild->nextElementSibling();
        if (!nextChild)
            break;
        selectedChild = nextChild;
    }

    return i;
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
                // For the "toggle" action type or any unknown action type, we rely on the value of the selection attribute to determine the visible child.
                getSelectedChildAndIndex(newSelectedChild);
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

void MathMLSelectElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        if (fastGetAttribute(MathMLNames::actiontypeAttr) == "toggle") {
            toggle();
            return;
        }
    }

    MathMLInlineContainerElement::defaultEventHandler(event);
}

bool MathMLSelectElement::willRespondToMouseClickEvents()
{
    return true;
}

void MathMLSelectElement::toggle()
{
    // We determine the successor of the selected child.
    // If we reach the end of the child list, we go back to the first child.
    Element* child = nullptr;
    int selection = getSelectedChildAndIndex(child);
    if (!child || !child->nextElementSibling())
        selection = 1;
    else
        selection++;

    // We update the attribute value of the selection attribute.
    // This will also call MathMLSelectElement::attributeChanged to update the selected child.
    setAttribute(MathMLNames::selectionAttr, AtomicString::number(selection));
}

}

#endif // ENABLE(MATHML)
