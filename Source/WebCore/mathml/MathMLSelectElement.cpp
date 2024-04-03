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

#include "ElementInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "MathMLNames.h"
#include "RenderMathMLRow.h"
#include "RenderTreeUpdater.h"
#include "SVGElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLSelectElement);

using namespace MathMLNames;

MathMLSelectElement::MathMLSelectElement(const QualifiedName& tagName, Document& document)
    : MathMLRowElement(tagName, document)
    , m_selectedChild(nullptr)
{
}

Ref<MathMLSelectElement> MathMLSelectElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLSelectElement(tagName, document));
}

RenderPtr<RenderElement> MathMLSelectElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderMathMLRow>(RenderObject::Type::MathMLRow, *this, WTFMove(style));
}

//  We recognize the following values for the encoding attribute of the <semantics> element:
//
// - "MathML-Presentation", which is mentioned in the MathML 3 recommendation.
// - "SVG1.1" which is mentioned in the W3C note.
//   http://www.w3.org/Math/Documents/Notes/graphics.xml
// - Other MIME Content-Types for MathML, SVG and HTML.
//
// We exclude "application/mathml+xml" which is ambiguous about whether it is Presentation or Content MathML. Authors must use a more explicit encoding value.
bool MathMLSelectElement::isMathMLEncoding(const AtomString& value)
{
    return value == "application/mathml-presentation+xml"_s || value == "MathML-Presentation"_s;
}

bool MathMLSelectElement::isSVGEncoding(const AtomString& value)
{
    return value == imageSVGContentTypeAtom() || value == "SVG1.1"_s;
}

bool MathMLSelectElement::isHTMLEncoding(const AtomString& value)
{
    return value == applicationXHTMLContentTypeAtom() || value == textHTMLContentTypeAtom();
}

bool MathMLSelectElement::childShouldCreateRenderer(const Node& child) const
{
    return MathMLElement::childShouldCreateRenderer(child) && m_selectedChild == &child;
}

void MathMLSelectElement::finishParsingChildren()
{
    updateSelectedChild();
    MathMLRowElement::finishParsingChildren();
}

void MathMLSelectElement::childrenChanged(const ChildChange& change)
{
    updateSelectedChild();
    MathMLRowElement::childrenChanged(change);
}

void MathMLSelectElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason reason)
{
    if (hasTagName(mactionTag) && (name == MathMLNames::actiontypeAttr || name == MathMLNames::selectionAttr))
        updateSelectedChild();

    MathMLRowElement::attributeChanged(name, oldValue, newValue, reason);
}

int MathMLSelectElement::getSelectedActionChildAndIndex(Element*& selectedChild)
{
    ASSERT(hasTagName(mactionTag));

    // We "round up or down to the closest allowable value" of the selection attribute, as suggested by the MathML specification.
    selectedChild = firstElementChild();
    if (!selectedChild)
        return 1;

    int selection = getIntegralAttribute(MathMLNames::selectionAttr);
    int i;
    for (i = 1; i < selection; i++) {
        auto* nextChild = selectedChild->nextElementSibling();
        if (!nextChild)
            break;
        selectedChild = nextChild;
    }

    return i;
}

Element* MathMLSelectElement::getSelectedActionChild()
{
    ASSERT(hasTagName(mactionTag));

    auto* child = firstElementChild();
    if (!child)
        return child;

    // The value of the actiontype attribute is case-sensitive.
    auto& actiontype = attributeWithoutSynchronization(MathMLNames::actiontypeAttr);
    if (actiontype == "statusline"_s) {
        // FIXME: implement user interaction for the "statusline" action type (http://wkbug/124922).
        { }
    } else if (actiontype == "tooltip"_s) {
        // FIXME: implement user interaction for the "tooltip" action type (http://wkbug/124921).
        { }
    } else {
        // For the "toggle" action type or any unknown action type, we rely on the value of the selection attribute to determine the visible child.
        getSelectedActionChildAndIndex(child);
    }

    return child;
}

Element* MathMLSelectElement::getSelectedSemanticsChild()
{
    ASSERT(hasTagName(semanticsTag));

    auto* child = firstElementChild();
    if (!child)
        return nullptr;

    if (!is<MathMLElement>(*child) || !downcast<MathMLElement>(*child).isPresentationMathML()) {
        // The first child is not a presentation MathML element. Hence we move to the second child and start searching an annotation child that could be displayed.
        child = child->nextElementSibling();
    } else if (!downcast<MathMLElement>(*child).isSemanticAnnotation()) {
        // The first child is a presentation MathML but not an annotation, so we can just display it.
        return child;
    }
    // Otherwise, the first child is an <annotation> or <annotation-xml> element. This is invalid, but some people use this syntax so we take care of this case too and start the search from this first child.

    for ( ; child; child = child->nextElementSibling()) {
        if (!is<MathMLElement>(*child))
            continue;

        if (child->hasTagName(MathMLNames::annotationTag)) {
            // If the <annotation> element has an src attribute then it is a reference to arbitrary binary data and it is not clear whether we can display it. Hence we just ignore the annotation.
            if (child->hasAttributeWithoutSynchronization(MathMLNames::srcAttr))
                continue;
            // Otherwise, we assume it is a text annotation that can always be displayed and we stop here.
            return child;
        }

        if (child->hasTagName(MathMLNames::annotation_xmlTag)) {
            // If the <annotation-xml> element has an src attribute then it is a reference to arbitrary binary data and it is not clear whether we can display it. Hence we just ignore the annotation.
            if (child->hasAttributeWithoutSynchronization(MathMLNames::srcAttr))
                continue;
            // If the <annotation-xml> element has an encoding attribute describing presentation MathML, SVG or HTML we assume the content can be displayed and we stop here.
            auto& value = child->attributeWithoutSynchronization(MathMLNames::encodingAttr);
            if (isMathMLEncoding(value) || isSVGEncoding(value) || isHTMLEncoding(value))
                return child;
        }
    }

    // We fallback to the first child.
    return firstElementChild();
}

void MathMLSelectElement::updateSelectedChild()
{
    auto* newSelectedChild = hasTagName(mactionTag) ? getSelectedActionChild() : getSelectedSemanticsChild();

    if (m_selectedChild == newSelectedChild)
        return;

    if (m_selectedChild && m_selectedChild->renderer())
        RenderTreeUpdater::tearDownRenderers(*m_selectedChild);

    m_selectedChild = newSelectedChild;
    invalidateStyleForSubtree();
}

void MathMLSelectElement::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent) {
        if (attributeWithoutSynchronization(MathMLNames::actiontypeAttr) == "toggle"_s) {
            toggle();
            event.setDefaultHandled();
            return;
        }
    }

    MathMLRowElement::defaultEventHandler(event);
}

bool MathMLSelectElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    return attributeWithoutSynchronization(MathMLNames::actiontypeAttr) == "toggle"_s || MathMLRowElement::willRespondToMouseClickEventsWithEditability(editability);
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
    setAttributeWithoutSynchronization(MathMLNames::selectionAttr, AtomString::number(newSelectedChildIndex));
}

}

#endif // ENABLE(MATHML)
