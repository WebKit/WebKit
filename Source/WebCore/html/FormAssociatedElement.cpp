/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "FormAssociatedElement.h"

#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "ValidityState.h"

namespace WebCore {

using namespace HTMLNames;

FormAssociatedElement::FormAssociatedElement()
    : m_form(0)
{
}

FormAssociatedElement::~FormAssociatedElement()
{
    setForm(0);
}

ValidityState* FormAssociatedElement::validity()
{
    if (!m_validityState)
        m_validityState = ValidityState::create(this);

    return m_validityState.get();
}

void FormAssociatedElement::didMoveToNewDocument(Document* oldDocument)
{
    HTMLElement* element = toHTMLElement(this);
    if (oldDocument && element->fastHasAttribute(formAttr))
        oldDocument->unregisterFormElementWithFormAttribute(this);
}

void FormAssociatedElement::insertedInto(Node* insertionPoint)
{
    resetFormOwner();
    if (!insertionPoint->inDocument())
        return;

    HTMLElement* element = toHTMLElement(this);
    if (element->fastHasAttribute(formAttr))
        element->document()->registerFormElementWithFormAttribute(this);
}

void FormAssociatedElement::removedFrom(Node* insertionPoint)
{
    HTMLElement* element = toHTMLElement(this);
    if (insertionPoint->inDocument() && element->fastHasAttribute(formAttr))
        element->document()->unregisterFormElementWithFormAttribute(this);
    // If the form and element are both in the same tree, preserve the connection to the form.
    // Otherwise, null out our form and remove ourselves from the form's list of elements.
    if (m_form && element->highestAncestor() != m_form->highestAncestor())
        setForm(0);
}

HTMLFormElement* FormAssociatedElement::findAssociatedForm(const HTMLElement* element, HTMLFormElement* currentAssociatedForm)
{
    const AtomicString& formId(element->fastGetAttribute(formAttr));
    if (!formId.isNull() && element->inDocument()) {
        // The HTML5 spec says that the element should be associated with
        // the first element in the document to have an ID that equal to
        // the value of form attribute, so we put the result of
        // treeScope()->getElementById() over the given element.
        HTMLFormElement* newForm = 0;
        Element* newFormCandidate = element->treeScope()->getElementById(formId);
        if (newFormCandidate && newFormCandidate->hasTagName(formTag))
            newForm = static_cast<HTMLFormElement*>(newFormCandidate);
        return newForm;
    }

    if (!currentAssociatedForm)
        return element->findFormAncestor();

    return currentAssociatedForm;
}

void FormAssociatedElement::formRemovedFromTree(const Node* formRoot)
{
    ASSERT(m_form);
    if (toHTMLElement(this)->highestAncestor() != formRoot)
        setForm(0);
}

void FormAssociatedElement::setForm(HTMLFormElement* newForm)
{
    if (m_form == newForm)
        return;
    willChangeForm();
    if (m_form)
        m_form->removeFormElement(this);
    m_form = newForm;
    if (m_form)
        m_form->registerFormElement(this);
    didChangeForm();
}

void FormAssociatedElement::willChangeForm()
{
}

void FormAssociatedElement::didChangeForm()
{
}

void FormAssociatedElement::formWillBeDestroyed()
{
    ASSERT(m_form);
    if (!m_form)
        return;
    willChangeForm();
    m_form = 0;
    didChangeForm();
}

void FormAssociatedElement::resetFormOwner()
{
    setForm(findAssociatedForm(toHTMLElement(this), m_form));
}

void FormAssociatedElement::formAttributeChanged()
{
    HTMLElement* element = toHTMLElement(this);
    if (!element->fastHasAttribute(formAttr)) {
        // The form attribute removed. We need to reset form owner here.
        setForm(element->findFormAncestor());
        element->document()->unregisterFormElementWithFormAttribute(this);
    } else
        resetFormOwner();
}

const HTMLElement* toHTMLElement(const FormAssociatedElement* associatedElement)
{
    if (associatedElement->isFormControlElement())
        return static_cast<const HTMLFormControlElement*>(associatedElement);
    // Assumes the element is an HTMLObjectElement
    const HTMLElement* element = static_cast<const HTMLObjectElement*>(associatedElement);
    ASSERT(element->hasTagName(objectTag));
    return element;
}

HTMLElement* toHTMLElement(FormAssociatedElement* associatedElement)
{
    return const_cast<HTMLElement*>(toHTMLElement(static_cast<const FormAssociatedElement*>(associatedElement)));
}

} // namespace Webcore
