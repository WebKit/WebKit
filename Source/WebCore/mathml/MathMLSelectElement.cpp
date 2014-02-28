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

RenderPtr<RenderElement> MathMLSelectElement::createElementRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderMathMLRow>(*this, std::move(style));
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

void MathMLSelectElement::attributeChanged(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason reason)
{
    if (hasLocalName(mactionTag) && (name == MathMLNames::actiontypeAttr || name == MathMLNames::selectionAttr))
        updateSelectedChild();

    MathMLInlineContainerElement::attributeChanged(name, oldValue, newValue, reason);
}

int MathMLSelectElement::getSelectedActionChildAndIndex(Element*& selectedChild)
{
    ASSERT(hasLocalName(mactionTag));

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

Element* MathMLSelectElement::getSelectedActionChild()
{
    ASSERT(hasLocalName(mactionTag));

    Element* child = firstElementChild();
    if (!child)
        return child;

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
        getSelectedActionChildAndIndex(child);
    }

    return child;
}

Element* MathMLSelectElement::getSelectedSemanticsChild()
{
    ASSERT(hasLocalName(semanticsTag));

    Element* child = firstElementChild();
    if (!child)
        return child;

    if (!child->isMathMLElement() || !toMathMLElement(child)->isPresentationMathML()) { 
        // The first child is not a presentation MathML element. Hence we move to the second child and start searching an annotation child that could be displayed.
        child = child->nextElementSibling();
    } else if (!toMathMLElement(child)->isSemanticAnnotation()) {
        // The first child is a presentation MathML but not an annotation, so we can just display it.
        return child;
    }
    // Otherwise, the first child is an <annotation> or <annotation-xml> element. This is invalid, but some people use this syntax so we take care of this case too and start the search from this first child.

    for ( ; child; child = child->nextElementSibling()) {
        if (!child->isMathMLElement())
            continue;

        if (child->hasLocalName(MathMLNames::annotationTag)) {
            // If the <annotation> element has an src attribute then it is a reference to arbitrary binary data and it is not clear whether we can display it. Hence we just ignore the annotation.
            if (child->hasAttribute(MathMLNames::srcAttr))
                continue;
            // Otherwise, we assume it is a text annotation that can always be displayed and we stop here.
            return child;
        }

        if (child->hasLocalName(MathMLNames::annotation_xmlTag)) {
            // If the <annotation-xml> element has an src attribute then it is a reference to arbitrary binary data and it is not clear whether we can display it. Hence we just ignore the annotation.
            if (child->hasAttribute(MathMLNames::srcAttr))
                continue;
            // If the <annotation-xml> element has an encoding attribute describing presentation MathML, SVG or HTML we assume the content can be displayed and we stop here. We recognize the following encoding values:
            //
            // - "MathML-Presentation", which is mentioned in the MathML 3 recommendation.
            // - "SVG1.1" which is mentioned in the W3C note.
            //   http://www.w3.org/Math/Documents/Notes/graphics.xml
            // - Other MIME Content-Types for SVG and HTML.
            //
            // We exclude "application/mathml+xml" which is ambiguous about whether it is Presentation or Content MathML. Authors must use a more explicit encoding value.
            const AtomicString& value = child->fastGetAttribute(MathMLNames::encodingAttr);
            if (value == "application/mathml-presentation+xml" || value == "MathML-Presentation" || value == "image/svg+xml" || value == "SVG1.1" || value == "application/xhtml+xml" || value == "text/html")
                return child;
        }
    }

    // We fallback to the first child.
    return firstElementChild();
}

void MathMLSelectElement::updateSelectedChild()
{
    Element* newSelectedChild = hasLocalName(mactionTag) ? getSelectedActionChild() : getSelectedSemanticsChild();

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
            event->setDefaultHandled();
            return;
        }
    }

    MathMLInlineContainerElement::defaultEventHandler(event);
}

bool MathMLSelectElement::willRespondToMouseClickEvents()
{
    return fastGetAttribute(MathMLNames::actiontypeAttr) == "toggle";
}

void MathMLSelectElement::toggle()
{
    // Select the successor of the currently selected child
    // or the first child if the currently selected child is the last.
    Element* selectedChild;
    int newSelectedChildIndex = getSelectedActionChildAndIndex(selectedChild) + 1;
    if (!selectedChild || !selectedChild->nextElementSibling())
        newSelectedChildIndex = 1;

    // We update the attribute value of the selection attribute.
    // This will also call MathMLSelectElement::attributeChanged to update the selected child.
    setAttribute(MathMLNames::selectionAttr, AtomicString::number(newSelectedChildIndex));
}

}

#endif // ENABLE(MATHML)
