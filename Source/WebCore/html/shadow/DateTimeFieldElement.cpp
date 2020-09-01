/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DateTimeFieldElement.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "DateComponents.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "PlatformLocale.h"
#include "Text.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeFieldElement);

DateTimeFieldElement::FieldOwner::~FieldOwner() = default;

DateTimeFieldElement::DateTimeFieldElement(Document& document, FieldOwner& fieldOwner)
    : HTMLDivElement(divTag, document)
    , m_fieldOwner(makeWeakPtr(fieldOwner))
{
}

void DateTimeFieldElement::initialize(const AtomString& pseudo)
{
    setPseudo(pseudo);
}

void DateTimeFieldElement::defaultEventHandler(Event& event)
{
    if (is<KeyboardEvent>(event)) {
        auto& keyboardEvent = downcast<KeyboardEvent>(event);
        handleKeyboardEvent(keyboardEvent);
        if (keyboardEvent.defaultHandled())
            return;
        defaultKeyboardEventHandler(keyboardEvent);
        if (keyboardEvent.defaultHandled())
            return;
    }

    HTMLDivElement::defaultEventHandler(event);
}

void DateTimeFieldElement::defaultKeyboardEventHandler(KeyboardEvent& keyboardEvent)
{
    if (keyboardEvent.type() != eventNames().keydownEvent)
        return;

    auto key = keyboardEvent.keyIdentifier();

    if (key == "Left" && m_fieldOwner && m_fieldOwner->focusOnPreviousField(*this)) {
        keyboardEvent.setDefaultHandled();
        return;
    }

    if (key == "Right" && m_fieldOwner && m_fieldOwner->focusOnNextField(*this)) {
        keyboardEvent.setDefaultHandled();
        return;
    }

    // Clear value when pressing backspace or delete.
    if (key == "U+0008" || key == "U+007F") {
        keyboardEvent.setDefaultHandled();
        setEmptyValue(DispatchInputAndChangeEvents);
        return;
    }
}

void DateTimeFieldElement::dispatchBlurEvent(RefPtr<Element>&& newFocusedElement)
{
    if (m_fieldOwner)
        m_fieldOwner->blurFromField(WTFMove(newFocusedElement));
    else
        HTMLElement::dispatchBlurEvent(WTFMove(newFocusedElement));

    didBlur();
}

void DateTimeFieldElement::didBlur()
{
}

Locale& DateTimeFieldElement::localeForOwner() const
{
    return document().getCachedLocale(localeIdentifier());
}

AtomString DateTimeFieldElement::localeIdentifier() const
{
    return m_fieldOwner ? m_fieldOwner->localeIdentifier() : nullAtom();
}

void DateTimeFieldElement::updateVisibleValue(EventBehavior eventBehavior)
{
    if (!firstChild())
        appendChild(Text::create(document(), emptyString()));

    auto& textNode = downcast<Text>(*firstChild());
    String newVisibleValue = visibleValue();
    if (textNode.wholeText() == newVisibleValue)
        return;

    textNode.replaceWholeText(newVisibleValue);

    if (eventBehavior == DispatchInputAndChangeEvents && m_fieldOwner)
        m_fieldOwner->fieldValueChanged();
}

bool DateTimeFieldElement::supportsFocus() const
{
    return true;
}

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
