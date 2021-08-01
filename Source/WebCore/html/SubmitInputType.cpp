/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#include "SubmitInputType.h"

#include "DOMFormData.h"
#include "Event.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "InputTypeNames.h"
#include "LocalizedStrings.h"

namespace WebCore {

const AtomString& SubmitInputType::formControlType() const
{
    return InputTypeNames::submit();
}

bool SubmitInputType::appendFormData(DOMFormData& formData, bool) const
{
    ASSERT(element());
    if (!element()->isActivatedSubmit())
        return false;
    formData.append(element()->name(), element()->valueWithDefault());
    return true;
}

bool SubmitInputType::supportsRequired() const
{
    return false;
}

void SubmitInputType::handleDOMActivateEvent(Event& event)
{
    ASSERT(element());
    Ref<HTMLInputElement> protectedElement(*element());
    if (protectedElement->isDisabledFormControl() || !protectedElement->form())
        return;

    Ref<HTMLFormElement> protectedForm(*protectedElement->form());

    // Update layout before processing form actions in case the style changes
    // the Form or button relationships.
    protectedElement->document().updateLayoutIgnorePendingStylesheets();

    protectedElement->setActivatedSubmit(true);
    if (auto currentForm = makeRefPtr(protectedElement->form()))
        currentForm->submitIfPossible(&event, element()); // Event handlers can run.
    protectedElement->setActivatedSubmit(false);
    event.setDefaultHandled();
}

bool SubmitInputType::canBeSuccessfulSubmitButton()
{
    return true;
}

String SubmitInputType::defaultValue() const
{
    return submitButtonDefaultLabel();
}

} // namespace WebCore
