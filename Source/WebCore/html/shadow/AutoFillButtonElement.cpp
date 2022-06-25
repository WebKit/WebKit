/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "AutoFillButtonElement.h"

#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "MouseEvent.h"
#include "TextFieldInputType.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AutoFillButtonElement);

using namespace HTMLNames;

Ref<AutoFillButtonElement> AutoFillButtonElement::create(Document& document, AutoFillButtonOwner& owner)
{
    return adoptRef(*new AutoFillButtonElement(document, owner));
}

AutoFillButtonElement::AutoFillButtonElement(Document& document, AutoFillButtonOwner& owner)
    : HTMLDivElement(divTag, document)
    , m_owner(owner)
{
}

void AutoFillButtonElement::defaultEventHandler(Event& event)
{
    if (!is<MouseEvent>(event)) {
        if (!event.defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    MouseEvent& mouseEvent = downcast<MouseEvent>(event);

    if (mouseEvent.type() == eventNames().clickEvent) {
        m_owner.autoFillButtonElementWasClicked();
        event.setDefaultHandled();
    }

    if (!event.defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

} // namespace WebCore
