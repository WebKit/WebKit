/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
#include "DateTimeFieldElement.h"

#include "DateComponents.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

DateTimeFieldElement::FieldEventHandler::~FieldEventHandler()
{
}

DateTimeFieldElement::DateTimeFieldElement(Document* document, FieldEventHandler& fieldEventHandler)
    : HTMLElement(spanTag, document)
    , m_fieldEventHandler(&fieldEventHandler)
{
}

void DateTimeFieldElement::defaultEventHandler(Event* event)
{
    if (event->isKeyboardEvent()) {
        KeyboardEvent* keyboardEvent = static_cast<KeyboardEvent*>(event);
        handleKeyboardEvent(keyboardEvent);
        if (keyboardEvent->defaultHandled())
            return;
        defaultKeyboardEventHandler(keyboardEvent);
        if (keyboardEvent->defaultHandled())
            return;
    }

    HTMLElement::defaultEventHandler(event);
}

void DateTimeFieldElement::defaultKeyboardEventHandler(KeyboardEvent* keyboardEvent)
{
    if (keyboardEvent->type() != eventNames().keydownEvent)
        return;

    const String& keyIdentifier = keyboardEvent->keyIdentifier();

    if (keyIdentifier == "Down") {
        keyboardEvent->setDefaultHandled();
        stepDown();
        return;
    }

    if (keyIdentifier == "Up") {
        keyboardEvent->setDefaultHandled();
        stepUp();
        return;
    }

    if (keyIdentifier == "U+0008") {
        keyboardEvent->setDefaultHandled();
        setEmptyValue(DateComponents(), DispatchEvent);
        return;
    }
}

void DateTimeFieldElement::focusOnNextField()
{
    if (m_fieldEventHandler)
        m_fieldEventHandler->focusOnNextField();
}

void DateTimeFieldElement::initialize(const AtomicString& shadowPseudoId)
{
    setShadowPseudoId(shadowPseudoId);
    appendChild(Text::create(document(), visibleValue()));
}

bool DateTimeFieldElement::isReadOnly() const
{
    return fastHasAttribute(readonlyAttr);
}

void DateTimeFieldElement::setReadOnly()
{
    // Set HTML attribute readonly to change apperance.
    setBooleanAttribute(readonlyAttr, true);
    setNeedsStyleRecalc();
}

void DateTimeFieldElement::updateVisibleValue(EventBehavior eventBehavior)
{
    Text* const textNode = toText(firstChild());
    const String newVisibleValue = visibleValue();
    ASSERT(newVisibleValue.length() > 0);

    if (textNode->wholeText() == newVisibleValue)
        return;

    textNode->replaceWholeText(newVisibleValue, ASSERT_NO_EXCEPTION);

    if (eventBehavior == DispatchEvent && m_fieldEventHandler)
        m_fieldEventHandler->fieldValueChanged();
}

double DateTimeFieldElement::valueAsDouble() const
{
    return hasValue() ? valueAsInteger() * unitInMillisecond() : std::numeric_limits<double>::quiet_NaN();
}

} // namespace WebCore

#endif
