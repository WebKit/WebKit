/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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
#include "FormListedElement.h"

#include "EditorClient.h"
#include "ElementAncestorIterator.h"
#include "ElementInlines.h"
#include "FormController.h"
#include "Frame.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "IdTargetObserver.h"

namespace WebCore {

using namespace HTMLNames;

class FormAttributeTargetObserver final : private IdTargetObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FormAttributeTargetObserver(const AtomString& id, FormListedElement&);

private:
    void idTargetChanged() override;

    FormListedElement& m_element;
};

FormListedElement::FormListedElement(HTMLFormElement* form)
    : m_form(nullptr)
    , m_formSetByParser(form)
{
}

FormListedElement::~FormListedElement()
{
    RELEASE_ASSERT(!m_form);
}

void FormListedElement::didMoveToNewDocument(Document&)
{
    HTMLElement& element = asHTMLElement();
    if (element.hasAttributeWithoutSynchronization(formAttr) && element.isConnected())
        resetFormAttributeTargetObserver();
}

void FormListedElement::insertedIntoAncestor(Node::InsertionType insertionType, ContainerNode&)
{
    HTMLElement& element = asHTMLElement();
    if (m_formSetByParser) {
        // The form could have been removed by a script during parsing.
        if (m_formSetByParser->isConnected())
            setForm(m_formSetByParser.get());
        m_formSetByParser = nullptr;
    }

    if (m_form && element.rootElement() != m_form->rootElement())
        setForm(nullptr);

    if (!insertionType.connectedToDocument)
        return;

    if (element.hasAttributeWithoutSynchronization(formAttr))
        resetFormAttributeTargetObserver();
}

void FormListedElement::removedFromAncestor(Node::RemovalType removalType, ContainerNode&)
{
    auto& element = asHTMLElement();
    m_formAttributeTargetObserver = nullptr;

    // If the form and element are both in the same tree, preserve the connection to the form.
    // Otherwise, null out our form and remove ourselves from the form's list of elements.
    // Do not rely on rootNode() because our IsInTreeScope is outdated.
    if ((m_form && &element.traverseToRootNode() != &m_form->traverseToRootNode())
        || (removalType.disconnectedFromDocument && element.hasAttributeWithoutSynchronization(formAttr))) {
        setForm(nullptr);
        resetFormOwner();
    }
}

HTMLFormElement* FormListedElement::findAssociatedForm(const HTMLElement* element, HTMLFormElement* currentAssociatedForm)
{
    const AtomString& formId(element->attributeWithoutSynchronization(formAttr));
    if (!formId.isNull() && element->isConnected()) {
        // The HTML5 spec says that the element should be associated with
        // the first element in the document to have an ID that equal to
        // the value of form attribute, so we put the result of
        // treeScope().getElementById() over the given element.
        RefPtr<Element> newFormCandidate = element->treeScope().getElementById(formId);
        if (!is<HTMLFormElement>(newFormCandidate))
            return nullptr;
        if (&element->traverseToRootNode() == &element->treeScope().rootNode()) {
            ASSERT(&element->traverseToRootNode() == &newFormCandidate->traverseToRootNode());
            return downcast<HTMLFormElement>(newFormCandidate.get());
        }
    }

    if (!currentAssociatedForm)
        return HTMLFormElement::findClosestFormAncestor(*element);

    return currentAssociatedForm;
}

HTMLFormElement* FormListedElement::form() const
{
    return m_form.get();
}

void FormListedElement::formOwnerRemovedFromTree(const Node& formRoot)
{
    ASSERT(m_form);
    // Can't use RefPtr here beacuse this function might be called inside ~ShadowRoot via addChildNodesToDeletionQueue. See webkit.org/b/189493.
    Node* rootNode = &asHTMLElement();
    for (auto* ancestor = asHTMLElement().parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        if (ancestor == m_form) {
            // Form is our ancestor so we don't need to reset our owner, we also no longer
            // need an id observer since we are no longer connected.
            m_formAttributeTargetObserver = nullptr;
            return;
        }
        rootNode = ancestor;
    }

    // We are no longer in the same tree as our form owner so clear our owner.
    if (rootNode != &formRoot)
        setForm(nullptr);
}

void FormListedElement::setForm(HTMLFormElement* newForm)
{
    if (m_form == newForm)
        return;
    willChangeForm();
    if (m_form)
        m_form->unregisterFormListedElement(*this);
    m_form = newForm;
    if (newForm)
        newForm->registerFormListedElement(*this);
    didChangeForm();
}

void FormListedElement::willChangeForm()
{
}

void FormListedElement::didChangeForm()
{
}

void FormListedElement::formWillBeDestroyed()
{
    ASSERT(m_form);
    if (!m_form)
        return;
    willChangeForm();
    m_form = nullptr;
    didChangeForm();
}

void FormListedElement::resetFormOwner()
{
    RefPtr<HTMLFormElement> originalForm = m_form.get();
    setForm(findAssociatedForm(&asHTMLElement(), originalForm.get()));
    HTMLElement& element = asHTMLElement();
    auto* newForm = m_form.get();
    if (newForm && newForm != originalForm && newForm->isConnected())
        element.document().didAssociateFormControl(element);
}

void FormListedElement::formAttributeChanged()
{
    HTMLElement& element = asHTMLElement();
    if (!element.hasAttributeWithoutSynchronization(formAttr)) {
        // The form attribute removed. We need to reset form owner here.
        RefPtr<HTMLFormElement> originalForm = m_form.get();
        // FIXME: Why does this not pass originalForm to findClosestFormAncestor?
        setForm(HTMLFormElement::findClosestFormAncestor(element));
        auto* newForm = m_form.get();
        if (newForm && newForm != originalForm && newForm->isConnected())
            element.document().didAssociateFormControl(element);
        m_formAttributeTargetObserver = nullptr;
    } else {
        resetFormOwner();
        if (element.isConnected())
            resetFormAttributeTargetObserver();
    }
}

bool FormListedElement::customError() const
{
    return !m_customValidationMessage.isEmpty();
}

bool FormListedElement::hasBadInput() const
{
    return false;
}

bool FormListedElement::patternMismatch() const
{
    return false;
}

bool FormListedElement::rangeOverflow() const
{
    return false;
}

bool FormListedElement::rangeUnderflow() const
{
    return false;
}

bool FormListedElement::stepMismatch() const
{
    return false;
}

bool FormListedElement::tooShort() const
{
    return false;
}

bool FormListedElement::tooLong() const
{
    return false;
}

bool FormListedElement::typeMismatch() const
{
    return false;
}

bool FormListedElement::computeValidity() const
{
    bool someError = typeMismatch() || stepMismatch() || rangeUnderflow() || rangeOverflow()
        || tooShort() || tooLong() || patternMismatch() || valueMissing() || hasBadInput() || customError();
    return !someError;
}

bool FormListedElement::valueMissing() const
{
    return false;
}

String FormListedElement::customValidationMessage() const
{
    return m_customValidationMessage;
}

String FormListedElement::validationMessage() const
{
    return willValidate() ? m_customValidationMessage : String();
}

void FormListedElement::setCustomValidity(const String& error)
{
    m_customValidationMessage = error;
}

void FormListedElement::resetFormAttributeTargetObserver()
{
    ASSERT_WITH_SECURITY_IMPLICATION(asHTMLElement().isConnected());
    m_formAttributeTargetObserver = makeUnique<FormAttributeTargetObserver>(asHTMLElement().attributeWithoutSynchronization(formAttr), *this);
}

void FormListedElement::formAttributeTargetChanged()
{
    resetFormOwner();
}

const AtomString& FormListedElement::name() const
{
    const AtomString& name = asHTMLElement().getNameAttribute();
    return name.isNull() ? emptyAtom() : name;
}

bool FormListedElement::isFormControlElementWithState() const
{
    return false;
}

FormAttributeTargetObserver::FormAttributeTargetObserver(const AtomString& id, FormListedElement& element)
    : IdTargetObserver(element.asHTMLElement().treeScope().idTargetObserverRegistry(), id)
    , m_element(element)
{
}

void FormAttributeTargetObserver::idTargetChanged()
{
    m_element.formAttributeTargetChanged();
}

} // namespace Webcore
