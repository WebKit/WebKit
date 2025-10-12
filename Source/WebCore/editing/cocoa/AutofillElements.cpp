/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "AutofillElements.h"

#include "DocumentPage.h"
#include "FocusController.h"
#include "FocusControllerTypes.h"
#include "Logging.h"
#include "Page.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AutofillElements);

static inline bool isAutofillableElement(Element& node)
{
    auto* inputElement = dynamicDowncast<HTMLInputElement>(node);
    return inputElement && (inputElement->isTextField() || inputElement->isEmailField());
}

static inline RefPtr<HTMLInputElement> nextAutofillableElement(Node* startNode, FocusController& focusController)
{
    RefPtr nextElement = dynamicDowncast<Element>(startNode);
    if (!nextElement)
        return nullptr;

    do {
        auto result = focusController.nextFocusableElement(*nextElement.get());
        if (!result.element && result.continuedSearchInRemoteFrame == ContinuedSearchInRemoteFrame::Yes)
            LOG(SiteIsolation, "Crossing site isolation process barrier searching for `nextAutofillableElement` is not yet supported");
        nextElement = result.element;
    } while (nextElement && !isAutofillableElement(*nextElement.get()));

    if (!nextElement)
        return nullptr;

    return &downcast<HTMLInputElement>(*nextElement);
}

static inline RefPtr<HTMLInputElement> previousAutofillableElement(Node* startNode, FocusController& focusController)
{
    RefPtr previousElement = dynamicDowncast<Element>(startNode);
    if (!previousElement)
        return nullptr;

    do {
        auto result = focusController.previousFocusableElement(*previousElement.get());
        if (!result.element && result.continuedSearchInRemoteFrame == ContinuedSearchInRemoteFrame::Yes)
            LOG(SiteIsolation, "Crossing site isolation process barrier searching for `previousAutofillableElement` is not yet supported");
        previousElement = result.element;
    } while (previousElement && !isAutofillableElement(*previousElement.get()));

    if (!previousElement)
        return nullptr;
    
    return &downcast<HTMLInputElement>(*previousElement);
}

AutofillElements::AutofillElements(RefPtr<HTMLInputElement>&& username, RefPtr<HTMLInputElement>&& password, RefPtr<HTMLInputElement>&& secondPassword)
    : m_username(WTFMove(username))
    , m_password(WTFMove(password))
    , m_secondPassword(WTFMove(secondPassword))
{
}

std::optional<AutofillElements> AutofillElements::computeAutofillElements(Ref<HTMLInputElement> start)
{
    if (!start->document().page())
        return std::nullopt;
    CheckedRef focusController = { start->document().page()->focusController() };
    if (start->isPasswordField()) {
        auto previousElement = previousAutofillableElement(start.ptr(), focusController);
        auto nextElement = nextAutofillableElement(start.ptr(), focusController);

        bool previousFieldIsTextField = previousElement && !previousElement->isPasswordField();
        bool hasSecondPasswordFieldToFill = nextElement && nextElement->isPasswordField() && nextElement->value()->isEmpty();

        // Always allow AutoFill in a password field, even if we fill information only into it.
        return {{ previousFieldIsTextField ? WTFMove(previousElement) : nullptr, WTFMove(start), hasSecondPasswordFieldToFill ? WTFMove(nextElement) : nullptr }};
    } else {
        RefPtr<HTMLInputElement> nextElement = nextAutofillableElement(start.ptr(), focusController);
        if (nextElement && is<HTMLInputElement>(*nextElement)) {
            if (nextElement->isPasswordField()) {
                auto elementAfterNextElement = nextAutofillableElement(nextElement.get(), focusController);
                bool hasSecondPasswordFieldToFill = elementAfterNextElement && elementAfterNextElement->isPasswordField() && elementAfterNextElement->value()->isEmpty();

                return {{ WTFMove(start), WTFMove(nextElement), hasSecondPasswordFieldToFill ? WTFMove(elementAfterNextElement) : nullptr }};
            }
        }
    }

    // Handle the case where a username field appears separately from a password field.
    auto autofillData = start->autofillData();
    if (toAutofillFieldName(autofillData.fieldName) == AutofillFieldName::Username || toAutofillFieldName(autofillData.fieldName) == AutofillFieldName::WebAuthn)
        return {{ WTFMove(start), nullptr, nullptr }};

    return std::nullopt;
}

void AutofillElements::autofill(String username, String password)
{
    if (m_username)
        m_username->setValueForUser(username);
    if (m_password)
        m_password->setValueForUser(password);
    if (m_secondPassword)
        m_secondPassword->setValueForUser(password);
}

} // namespace WebCore
