/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLMaybeFormAssociatedCustomElement.h"

#include "Document.h"
#include "ElementRareData.h"
#include "FormAssociatedCustomElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLMaybeFormAssociatedCustomElement);

using namespace HTMLNames;

HTMLMaybeFormAssociatedCustomElement::HTMLMaybeFormAssociatedCustomElement(const QualifiedName& tagName, Document& document)
    : HTMLElement { tagName, document, TypeFlag::HasDidMoveToNewDocument }
{
    ASSERT(Document::validateCustomElementName(tagName.localName()) == CustomElementNameValidationStatus::Valid);
}

HTMLMaybeFormAssociatedCustomElement::~HTMLMaybeFormAssociatedCustomElement() = default;

Ref<HTMLMaybeFormAssociatedCustomElement> HTMLMaybeFormAssociatedCustomElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLMaybeFormAssociatedCustomElement(tagName, document));
}

bool HTMLMaybeFormAssociatedCustomElement::isFormListedElement() const
{
    return isFormAssociatedCustomElement();
}

bool HTMLMaybeFormAssociatedCustomElement::isValidatedFormListedElement() const
{
    return isFormAssociatedCustomElement();
}

bool HTMLMaybeFormAssociatedCustomElement::isFormAssociatedCustomElement() const
{
    return hasFormAssociatedInterface() && isDefinedCustomElement();
}

FormAssociatedElement* HTMLMaybeFormAssociatedCustomElement::asFormAssociatedElement()
{
    return isFormAssociatedCustomElement() ? &formAssociatedCustomElementUnsafe() : nullptr;
}

FormListedElement* HTMLMaybeFormAssociatedCustomElement::asFormListedElement()
{
    return isFormAssociatedCustomElement() ? &formAssociatedCustomElementUnsafe() : nullptr;
}

ValidatedFormListedElement* HTMLMaybeFormAssociatedCustomElement::asValidatedFormListedElement()
{
    return isFormAssociatedCustomElement() ? &formAssociatedCustomElementUnsafe() : nullptr;
}

FormAssociatedCustomElement* HTMLMaybeFormAssociatedCustomElement::formAssociatedCustomElementForElementInternals() const
{
    return hasFormAssociatedInterface() && isPrecustomizedOrDefinedCustomElement() ? &formAssociatedCustomElementUnsafe() : nullptr;
}

bool HTMLMaybeFormAssociatedCustomElement::matchesValidPseudoClass() const
{
    return isFormAssociatedCustomElement() && formAssociatedCustomElementUnsafe().matchesValidPseudoClass();
}

bool HTMLMaybeFormAssociatedCustomElement::matchesInvalidPseudoClass() const
{
    return isFormAssociatedCustomElement() && formAssociatedCustomElementUnsafe().matchesInvalidPseudoClass();
}

bool HTMLMaybeFormAssociatedCustomElement::matchesUserValidPseudoClass() const
{
    return isFormAssociatedCustomElement() && formAssociatedCustomElementUnsafe().matchesUserValidPseudoClass();
}

bool HTMLMaybeFormAssociatedCustomElement::matchesUserInvalidPseudoClass() const
{
    return isFormAssociatedCustomElement() && formAssociatedCustomElementUnsafe().matchesUserInvalidPseudoClass();
}

bool HTMLMaybeFormAssociatedCustomElement::supportsFocus() const
{
    return isFormAssociatedCustomElement() ? (shadowRoot() && shadowRoot()->delegatesFocus()) || (HTMLElement::supportsFocus() && !formAssociatedCustomElementUnsafe().isDisabled()) : HTMLElement::supportsFocus();
}

bool HTMLMaybeFormAssociatedCustomElement::isLabelable() const
{
    return isFormAssociatedCustomElement();
}

bool HTMLMaybeFormAssociatedCustomElement::isDisabledFormControl() const
{
    return isFormAssociatedCustomElement() && formAssociatedCustomElementUnsafe().isDisabled();
}

Node::InsertedIntoAncestorResult HTMLMaybeFormAssociatedCustomElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (isFormAssociatedCustomElement())
        formAssociatedCustomElementUnsafe().insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLMaybeFormAssociatedCustomElement::didFinishInsertingNode()
{
    HTMLElement::didFinishInsertingNode();
    if (isFormAssociatedCustomElement())
        formAssociatedCustomElementUnsafe().didFinishInsertingNode();
}

void HTMLMaybeFormAssociatedCustomElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    if (isFormAssociatedCustomElement())
        formAssociatedCustomElementUnsafe().didMoveToNewDocument();
}

void HTMLMaybeFormAssociatedCustomElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (isFormAssociatedCustomElement())
        formAssociatedCustomElementUnsafe().removedFromAncestor(removalType, oldParentOfRemovedTree);
}

void HTMLMaybeFormAssociatedCustomElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
    if (isFormAssociatedCustomElement())
        formAssociatedCustomElementUnsafe().parseAttribute(name, newValue);
}

void HTMLMaybeFormAssociatedCustomElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    if (isFormAssociatedCustomElement())
        formAssociatedCustomElementUnsafe().finishParsingChildren();
}

void HTMLMaybeFormAssociatedCustomElement::setInterfaceIsFormAssociated()
{
    setEventTargetFlag(EventTargetFlag::HasFormAssociatedCustomElementInterface, true);
    ensureFormAssociatedCustomElement();
}

void HTMLMaybeFormAssociatedCustomElement::willUpgradeFormAssociated()
{
    ASSERT(isPrecustomizedCustomElement());
    setInterfaceIsFormAssociated();
    formAssociatedCustomElementUnsafe().willUpgrade();
}

void HTMLMaybeFormAssociatedCustomElement::didUpgradeFormAssociated()
{
    ASSERT(isFormAssociatedCustomElement());
    formAssociatedCustomElementUnsafe().didUpgrade();
}

} // namespace WebCore
