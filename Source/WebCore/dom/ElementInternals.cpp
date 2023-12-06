/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ElementInternals.h"
#include "CustomStateSet.h"

#include "AXObjectCache.h"
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "HTMLFormElement.h"
#include "HTMLMaybeFormAssociatedCustomElement.h"
#include "ShadowRoot.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(ElementInternals);

RefPtr<ShadowRoot> ElementInternals::shadowRoot() const
{
    RefPtr element = m_element.get();
    if (!element)
        return nullptr;
    RefPtr shadowRoot = element->shadowRoot();
    if (!shadowRoot)
        return nullptr;
    if (!shadowRoot->isAvailableToElementInternals())
        return nullptr;
    return shadowRoot;
}

ExceptionOr<RefPtr<HTMLFormElement>> ElementInternals::form() const
{
    if (RefPtr element = elementAsFormAssociatedCustom())
        return element->form();
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<void> ElementInternals::setFormValue(CustomElementFormValue&& value, std::optional<CustomElementFormValue>&& state)
{
    if (RefPtr element = elementAsFormAssociatedCustom()) {
        element->setFormValue(WTFMove(value), WTFMove(state));
        return { };
    }

    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<void> ElementInternals::setValidity(ValidityStateFlags validityStateFlags, String&& message, HTMLElement* validationAnchor)
{
    if (RefPtr element = elementAsFormAssociatedCustom())
        return element->setValidity(validityStateFlags, WTFMove(message), validationAnchor);
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<bool> ElementInternals::willValidate() const
{
    if (RefPtr element = elementAsFormAssociatedCustom())
        return element->willValidate();
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<RefPtr<ValidityState>> ElementInternals::validity()
{
    if (RefPtr element = elementAsFormAssociatedCustom())
        return element->validity();
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<String> ElementInternals::validationMessage() const
{
    if (RefPtr element = elementAsFormAssociatedCustom())
        return element->validationMessage();
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<bool> ElementInternals::reportValidity()
{
    if (RefPtr element = elementAsFormAssociatedCustom())
        return element->reportValidity();
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<bool> ElementInternals::checkValidity()
{
    if (RefPtr element = elementAsFormAssociatedCustom())
        return element->checkValidity();
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<RefPtr<NodeList>> ElementInternals::labels()
{
    if (RefPtr element = elementAsFormAssociatedCustom())
        return element->asHTMLElement().labels();
    return Exception { ExceptionCode::NotSupportedError };
}

FormAssociatedCustomElement* ElementInternals::elementAsFormAssociatedCustom() const
{
    if (RefPtr element = dynamicDowncast<HTMLMaybeFormAssociatedCustomElement>(m_element.get()))
        return element->formAssociatedCustomElementForElementInternals();
    return nullptr;
}

static const AtomString& computeValueForAttribute(Element& element, const QualifiedName& name)
{
    auto& value = element.attributeWithoutSynchronization(name);
    if (CheckedPtr defaultARIA = element.customElementDefaultARIAIfExists(); value.isNull() && defaultARIA)
        return defaultARIA->valueForAttribute(element, name);
    return value;
}

void ElementInternals::setAttributeWithoutSynchronization(const QualifiedName& name, const AtomString& value)
{
    RefPtr element = m_element.get();
    auto oldValue = computeValueForAttribute(*element, name);

    element->checkedCustomElementDefaultARIA()->setValueForAttribute(name, value);

    if (CheckedPtr cache = element->document().existingAXObjectCache())
        cache->deferAttributeChangeIfNeeded(element.get(), name, oldValue, computeValueForAttribute(*element, name));
}

const AtomString& ElementInternals::attributeWithoutSynchronization(const QualifiedName& name) const
{
    RefPtr element = m_element.get();
    CheckedPtr defaultARIA = element->customElementDefaultARIAIfExists();
    return defaultARIA ? defaultARIA->valueForAttribute(*element, name) : nullAtom();
}

RefPtr<Element> ElementInternals::getElementAttribute(const QualifiedName& name) const
{
    RefPtr element = m_element.get();
    CheckedPtr defaultARIA = m_element->customElementDefaultARIAIfExists();
    return defaultARIA ? defaultARIA->elementForAttribute(*element, name) : nullptr;
}

void ElementInternals::setElementAttribute(const QualifiedName& name, Element* value)
{
    RefPtr element = m_element.get();
    auto oldValue = computeValueForAttribute(*element, name);

    element->checkedCustomElementDefaultARIA()->setElementForAttribute(name, value);

    if (CheckedPtr cache = element->document().existingAXObjectCache())
        cache->deferAttributeChangeIfNeeded(element.get(), name, oldValue, computeValueForAttribute(*element, name));
}

std::optional<Vector<RefPtr<Element>>> ElementInternals::getElementsArrayAttribute(const QualifiedName& name) const
{
    RefPtr element = m_element.get();
    CheckedPtr defaultARIA = m_element->customElementDefaultARIAIfExists();
    if (!defaultARIA)
        return std::nullopt;
    return defaultARIA->elementsForAttribute(*element, name);
}

void ElementInternals::setElementsArrayAttribute(const QualifiedName& name, std::optional<Vector<RefPtr<Element>>>&& value)
{
    RefPtr element = m_element.get();
    auto oldValue = computeValueForAttribute(*element, name);

    element->checkedCustomElementDefaultARIA()->setElementsForAttribute(name, WTFMove(value));

    if (CheckedPtr cache = element->document().existingAXObjectCache())
        cache->deferAttributeChangeIfNeeded(element.get(), name, oldValue, computeValueForAttribute(*element, name));
}

CustomStateSet& ElementInternals::states()
{
    return m_element->ensureCustomStateSet();
}

} // namespace WebCore
