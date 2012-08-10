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
#include "DateTimeNumericFieldElement.h"

#include "KeyboardEvent.h"
#include "LocalizedNumber.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static const DOMTimeStamp typeAheadTimeout = 1000;

static size_t displaySizeOfNumber(int value)
{
    ASSERT(value >= 0);

    if (!value)
        return 1;

    int numberOfDigits = 0;
    while (value) {
        value /= 10;
        ++numberOfDigits;
    }

    return numberOfDigits;
}

DateTimeNumericFieldElement::Range::Range(int minimum, int maximum)
    : maximum(maximum)
    , minimum(minimum)
{
    ASSERT(minimum <= maximum);
}

int DateTimeNumericFieldElement::Range::clampValue(int value) const
{
    return std::min(std::max(value, minimum), maximum);
}

DateTimeNumericFieldElement::DateTimeNumericFieldElement(Document* document, FieldEventHandler& fieldEventHandler, int minimum, int maximum)
    : DateTimeFieldElement(document, fieldEventHandler)
    , m_lastDigitCharTime(0)
    , m_range(minimum, maximum)
    , m_value(0)
    , m_hasValue(false)
{
}

void DateTimeNumericFieldElement::handleKeyboardEvent(KeyboardEvent* keyboardEvent)
{
    if (isReadOnly())
        return;

    if (keyboardEvent->type() != eventNames().keypressEvent)
        return;

    UChar charCode = static_cast<UChar>(keyboardEvent->charCode());
    if (charCode < ' ')
        return;

    DOMTimeStamp delta = keyboardEvent->timeStamp() - m_lastDigitCharTime;
    m_lastDigitCharTime = 0;

    String number = convertFromLocalizedNumber(String(&charCode, 1));
    const int digit = number[0] - '0';
    if (digit < 0 || digit > 9)
        return;

    keyboardEvent->setDefaultHandled();
    setValueAsInteger(m_hasValue && delta < typeAheadTimeout ? m_value * 10 + digit : digit, DispatchEvent);
    if (m_value * 10 > m_range.maximum)
        focusOnNextField();
    else
        m_lastDigitCharTime = keyboardEvent->timeStamp();
}

bool DateTimeNumericFieldElement::hasValue() const
{
    return m_hasValue;
}

void DateTimeNumericFieldElement::setEmptyValue(const DateComponents& dateForReadOnlyField, EventBehavior eventBehavior)
{
    m_lastDigitCharTime = 0;

    if (isReadOnly()) {
        setValueAsDate(dateForReadOnlyField);
        return;
    }

    m_hasValue = false;
    m_value = 0;
    updateVisibleValue(eventBehavior);
}

void DateTimeNumericFieldElement::setValueAsInteger(int value, EventBehavior eventBehavior)
{
    m_value = clampValue(value);
    m_hasValue = true;
    updateVisibleValue(eventBehavior);
    m_lastDigitCharTime = 0;
}

void DateTimeNumericFieldElement::stepDown()
{
    if (m_hasValue)
        setValueAsInteger(m_value == m_range.minimum ? m_range.maximum : clampValue(m_value - 1), DispatchEvent);
    else
        setValueAsInteger(m_range.maximum, DispatchEvent);
}

void DateTimeNumericFieldElement::stepUp()
{
    if (m_hasValue)
        setValueAsInteger(m_value == m_range.maximum ? m_range.minimum : clampValue(m_value + 1), DispatchEvent);
    else
        setValueAsInteger(m_range.minimum, DispatchEvent);
}

String DateTimeNumericFieldElement::value() const
{
    if (!m_hasValue)
        return emptyString();

    if (m_range.maximum > 999)
        return convertToLocalizedNumber(String::number(m_value));

    if (m_range.maximum > 99)
        return convertToLocalizedNumber(String::format("%03d", m_value));

    return convertToLocalizedNumber(String::format("%02d", m_value));
}

int DateTimeNumericFieldElement::valueAsInteger() const
{
    return m_hasValue ? m_value : -1;
}

String DateTimeNumericFieldElement::visibleValue() const
{
    if (m_hasValue)
        return value();

    StringBuilder builder;
    for (int numberOfDashs = std::max(displaySizeOfNumber(m_range.maximum), displaySizeOfNumber(m_range.minimum)); numberOfDashs; --numberOfDashs)
        builder.append('-');
    return builder.toString();
}

} // namespace WebCore

#endif
