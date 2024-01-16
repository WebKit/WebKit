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
#include "DateTimeNumericFieldElement.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "EventNames.h"
#include "FontCascade.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "PlatformLocale.h"
#include "RenderBlock.h"
#include "RenderStyleSetters.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

constexpr Seconds typeAheadTimeout { 1_s };

int DateTimeNumericFieldElement::Range::clampValue(int value) const
{
    return std::clamp(value, minimum, maximum);
}

bool DateTimeNumericFieldElement::Range::isInRange(int value) const
{
    return value >= minimum && value <= maximum;
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeNumericFieldElement);

DateTimeNumericFieldElement::DateTimeNumericFieldElement(Document& document, FieldOwner& fieldOwner, const Range& range, int placeholder)
    : DateTimeFieldElement(document, fieldOwner)
    , m_range(range)
    , m_placeholder(formatValue(placeholder))
{
}

void DateTimeNumericFieldElement::adjustMinInlineSize(RenderStyle& style) const
{
    auto& font = style.fontCascade();

    unsigned length = 2;
    if (m_range.maximum > 999)
        length = 4;
    else if (m_range.maximum > 99)
        length = 3;

    auto& locale = localeForOwner();

    float inlineSize = 0;
    for (char c = '0'; c <= '9'; ++c) {
        auto numberString = locale.convertToLocalizedNumber(makeString(pad(c, length, makeString(c))));
        inlineSize = std::max(inlineSize, font.width(RenderBlock::constructTextRun(numberString, style)));
    }

    if (style.isHorizontalWritingMode())
        style.setMinWidth({ inlineSize, LengthType::Fixed });
    else
        style.setMinHeight({ inlineSize, LengthType::Fixed });
}

int DateTimeNumericFieldElement::maximum() const
{
    return m_range.maximum;
}

String DateTimeNumericFieldElement::formatValue(int value) const
{
    Locale& locale = localeForOwner();

    if (m_range.maximum > 999)
        return locale.convertToLocalizedNumber(makeString(pad('0', 4, value)));
    if (m_range.maximum > 99)
        return locale.convertToLocalizedNumber(makeString(pad('0', 3, value)));
    return locale.convertToLocalizedNumber(makeString(pad('0', 2, value)));
}

bool DateTimeNumericFieldElement::hasValue() const
{
    return m_hasValue;
}

void DateTimeNumericFieldElement::setEmptyValue(EventBehavior eventBehavior)
{
    m_value = 0;
    m_hasValue = false;
    m_typeAheadBuffer.clear();
    updateVisibleValue(eventBehavior);
    setARIAValueAttributesWithInteger(0);
}

void DateTimeNumericFieldElement::setValueAsInteger(int value, EventBehavior eventBehavior)
{
    m_value = m_range.clampValue(value);
    m_hasValue = true;
    updateVisibleValue(eventBehavior);
    setARIAValueAttributesWithInteger(value);
}

void DateTimeNumericFieldElement::setValueAsIntegerByStepping(int value)
{
    m_typeAheadBuffer.clear();
    setValueAsInteger(value, DispatchInputAndChangeEvents);
}

void DateTimeNumericFieldElement::setARIAValueAttributesWithInteger(int value)
{
    setAttributeWithoutSynchronization(HTMLNames::aria_valuenowAttr, AtomString::number(value));
    setAttributeWithoutSynchronization(HTMLNames::aria_valuetextAttr, AtomString::number(value));
}

void DateTimeNumericFieldElement::stepDown()
{
    int newValue = m_hasValue ? m_value - 1 : m_range.maximum;
    if (!m_range.isInRange(newValue))
        newValue = m_range.maximum;
    setValueAsIntegerByStepping(newValue);
}

void DateTimeNumericFieldElement::stepUp()
{
    int newValue = m_hasValue ? m_value + 1 : m_range.minimum;
    if (!m_range.isInRange(newValue))
        newValue = m_range.minimum;
    setValueAsIntegerByStepping(newValue);
}

String DateTimeNumericFieldElement::value() const
{
    return m_hasValue ? formatValue(m_value) : emptyString();
}

String DateTimeNumericFieldElement::placeholderValue() const
{
    return m_placeholder;
}

int DateTimeNumericFieldElement::valueAsInteger() const
{
    return m_hasValue ? m_value : -1;
}

void DateTimeNumericFieldElement::handleKeyboardEvent(KeyboardEvent& keyboardEvent)
{
    if (keyboardEvent.type() != eventNames().keypressEvent)
        return;

    auto charCode = static_cast<UChar>(keyboardEvent.charCode());
    String number = localeForOwner().convertFromLocalizedNumber(String(&charCode, 1));
    int digit = number[0] - '0';
    if (digit < 0 || digit > 9)
        return;

    Seconds timeSinceLastDigitChar = keyboardEvent.timeStamp() - m_lastDigitCharTime;
    m_lastDigitCharTime = keyboardEvent.timeStamp();

    if (timeSinceLastDigitChar > typeAheadTimeout) {
        m_typeAheadBuffer.clear();
    } else if (auto length = m_typeAheadBuffer.length()) {
        unsigned maxLength = formatValue(m_range.maximum).length();
        if (length == maxLength)
            m_typeAheadBuffer.clear();
    }

    m_typeAheadBuffer.append(number);
    setValueAsInteger(parseIntegerAllowingTrailingJunk<int>(m_typeAheadBuffer).value_or(0), DispatchInputAndChangeEvents);

    keyboardEvent.setDefaultHandled();
}

void DateTimeNumericFieldElement::handleBlurEvent(Event& event)
{
    m_typeAheadBuffer.clear();
    DateTimeFieldElement::handleBlurEvent(event);
}

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
