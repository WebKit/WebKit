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
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeNumericFieldElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Font.h"
#include "KeyboardEvent.h"
#include "PlatformLocale.h"
#include <wtf/text/StringBuilder.h>

using namespace WTF::Unicode;

namespace WebCore {

static const DOMTimeStamp typeAheadTimeout = 1000;

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

bool DateTimeNumericFieldElement::Range::isInRange(int value) const
{
    return value >= minimum && value <= maximum;
}

// ----------------------------

DateTimeNumericFieldElement::DateTimeNumericFieldElement(Document* document, FieldOwner& fieldOwner, int minimum, int maximum, const String& placeholder, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeFieldElement(document, fieldOwner)
    , m_lastDigitCharTime(0)
    , m_placeholder(placeholder)
    , m_range(minimum, maximum)
    , m_value(0)
    , m_hasValue(false)
    , m_step(parameters.step)
    , m_stepBase(parameters.stepBase)
{
    ASSERT(m_step);

    // We show a direction-neutral string such as "--" as a placeholder. It
    // should follow the direction of numeric values.
    if (localeForOwner().isRTL()) {
        Direction dir = direction(formatValue(this->maximum())[0]);
        if (dir == LeftToRight || dir == EuropeanNumber || dir == ArabicNumber) {
            setInlineStyleProperty(CSSPropertyUnicodeBidi, CSSValueBidiOverride);
            setInlineStyleProperty(CSSPropertyDirection, CSSValueLtr);
        }
    }
}

int DateTimeNumericFieldElement::clampValueForHardLimits(int value) const
{
    return clampValue(value);
}

float DateTimeNumericFieldElement::maximumWidth(const Font& font)
{
    float maximumWidth = font.width(m_placeholder);
    maximumWidth = std::max(maximumWidth, font.width(formatValue(maximum())));
    maximumWidth = std::max(maximumWidth, font.width(value()));
    return maximumWidth + DateTimeFieldElement::maximumWidth(font);
}

int DateTimeNumericFieldElement::defaultValueForStepDown() const
{
    return m_range.maximum;
}

int DateTimeNumericFieldElement::defaultValueForStepUp() const
{
    return m_range.minimum;
}

void DateTimeNumericFieldElement::didBlur()
{
    m_lastDigitCharTime = 0;
    DateTimeFieldElement::didBlur();
}

String DateTimeNumericFieldElement::formatValue(int value) const
{
    Locale& locale = localeForOwner();
    if (m_range.maximum > 999)
        return locale.convertToLocalizedNumber(String::format("%04d", value));
    if (m_range.maximum > 99)
        return locale.convertToLocalizedNumber(String::format("%03d", value));
    return locale.convertToLocalizedNumber(String::format("%02d", value));
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

    String number = localeForOwner().convertFromLocalizedNumber(String(&charCode, 1));
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

int DateTimeNumericFieldElement::maximum() const
{
    return m_range.maximum;
}

int DateTimeNumericFieldElement::minimum() const
{
    return m_range.minimum;
}

void DateTimeNumericFieldElement::setEmptyValue(EventBehavior eventBehavior)
{
    m_lastDigitCharTime = 0;

    if (isReadOnly())
        return;

    m_hasValue = false;
    m_value = 0;
    updateVisibleValue(eventBehavior);
}

void DateTimeNumericFieldElement::setValueAsInteger(int value, EventBehavior eventBehavior)
{
    m_value = clampValueForHardLimits(value);
    m_hasValue = true;
    updateVisibleValue(eventBehavior);
    m_lastDigitCharTime = 0;
}

void DateTimeNumericFieldElement::stepDown()
{
    int newValue = roundDown(m_hasValue ? m_value - 1 : defaultValueForStepDown());
    if (!m_range.isInRange(newValue))
        newValue = roundDown(m_range.maximum);
    setValueAsInteger(newValue, DispatchEvent);
}

void DateTimeNumericFieldElement::stepUp()
{
    int newValue = roundUp(m_hasValue ? m_value + 1 : defaultValueForStepUp());
    if (!m_range.isInRange(newValue))
        newValue = roundUp(m_range.minimum);
    setValueAsInteger(newValue, DispatchEvent);
}

String DateTimeNumericFieldElement::value() const
{
    return m_hasValue ? formatValue(m_value) : emptyString();
}

int DateTimeNumericFieldElement::valueAsInteger() const
{
    return m_hasValue ? m_value : -1;
}

String DateTimeNumericFieldElement::visibleValue() const
{
    return m_hasValue ? value() : m_placeholder;
}

int DateTimeNumericFieldElement::roundDown(int n) const
{
    n -= m_stepBase;
    if (n >= 0)
        n = n / m_step * m_step;
    else
        n = -((-n + m_step - 1) / m_step * m_step);
    return n + m_stepBase;
}

int DateTimeNumericFieldElement::roundUp(int n) const
{
    n -= m_stepBase;
    if (n >= 0)
        n = (n + m_step - 1) / m_step * m_step;
    else
        n = -(-n / m_step * m_step);
    return n + m_stepBase;
}

} // namespace WebCore

#endif
