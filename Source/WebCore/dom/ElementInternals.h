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

#pragma once

#include "CustomElementFormValue.h"
#include "HTMLElement.h"
#include "ScriptWrappable.h"
#include "ValidityState.h"
#include "ValidityStateFlags.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class HTMLFormElement;
class FormAssociatedCustomElement;

class ElementInternals final : public ScriptWrappable, public RefCounted<ElementInternals> {
    WTF_MAKE_ISO_ALLOCATED(ElementInternals);
public:
    static Ref<ElementInternals> create(HTMLElement& element)
    {
        return adoptRef(*new ElementInternals(element));
    }

    Element* element() const { return m_element.get(); }
    ShadowRoot* shadowRoot() const;

    ExceptionOr<RefPtr<HTMLFormElement>> form() const;

    ExceptionOr<void> setFormValue(std::optional<CustomElementFormValue>&&, std::optional<CustomElementFormValue>&& = std::nullopt);

    ExceptionOr<void> setValidity(ValidityStateFlags, String&& message, HTMLElement* validationAnchor);
    ExceptionOr<bool> willValidate() const;
    ExceptionOr<RefPtr<ValidityState>> validity();
    ExceptionOr<String> validationMessage() const;
    ExceptionOr<bool> reportValidity();
    ExceptionOr<bool> checkValidity();

    ExceptionOr<RefPtr<NodeList>> labels();

    // For ARIAMixin
    const AtomString& attributeWithoutSynchronization(const QualifiedName&) const;
    void setAttributeWithoutSynchronization(const QualifiedName&, const AtomString& value);

    Element* getElementAttribute(const QualifiedName&) const;
    void setElementAttribute(const QualifiedName&, Element*);
    std::optional<Vector<RefPtr<Element>>> getElementsArrayAttribute(const QualifiedName&) const;
    void setElementsArrayAttribute(const QualifiedName&, std::optional<Vector<RefPtr<Element>>>&&);

private:
    ElementInternals(HTMLElement& element)
        : m_element(element)
    {
    }

    FormAssociatedCustomElement* elementAsFormAssociatedCustom() const;

    WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData> m_element;
};

} // namespace WebCore
