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
#include "DateTimeFieldElements.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "DateComponents.h"
#include "DateTimeFieldsState.h"
#include "HTMLNames.h"
#include "LocalizedStrings.h"
#include "ScriptDisallowedScope.h"
#include "UserAgentParts.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeDayFieldElement);

DateTimeDayFieldElement::DateTimeDayFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, Range(1, 31), fieldOwner.placeholderDate().monthDay())
{
}

Ref<DateTimeDayFieldElement> DateTimeDayFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    auto element = adoptRef(*new DateTimeDayFieldElement(document, fieldOwner));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditDayField());
    element->setAttributeWithoutSynchronization(HTMLNames::aria_labelAttr, AtomString { AXDateFieldDayText() });
    element->setAttributeWithoutSynchronization(HTMLNames::roleAttr, AtomString { "spinbutton"_s });
    return element;
}

void DateTimeDayFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& state)
{
    if (hasValue())
        state.dayOfMonth = valueAsInteger();
}

void DateTimeDayFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.monthDay());
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeHourFieldElement);

DateTimeHourFieldElement::DateTimeHourFieldElement(Document& document, FieldOwner& fieldOwner, int minimum, int maximum)
    : DateTimeNumericFieldElement(document, fieldOwner, Range(minimum, maximum), (maximum >= 12) ? 12 : 11)
{
}

Ref<DateTimeHourFieldElement> DateTimeHourFieldElement::create(Document& document, FieldOwner& fieldOwner, int minimum, int maximum)
{
    auto element = adoptRef(*new DateTimeHourFieldElement(document, fieldOwner, minimum, maximum));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditHourField());
    return element;
}

void DateTimeHourFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& state)
{
    if (!hasValue())
        return;

    int value = valueAsInteger();

    switch (maximum()) {
    case 11:
        state.hour = value ?: 12;
        return;
    case 12:
        state.hour = value;
        return;
    case 23:
        state.hour = (value % 12) ?: 12;
        state.meridiem = value >= 12 ? DateTimeFieldsState::Meridiem::PM : DateTimeFieldsState::Meridiem::AM;
        return;
    case 24:
        if (value == 24) {
            state.hour = 12;
            state.meridiem = DateTimeFieldsState::Meridiem::AM;
            return;
        }
        state.hour = (value % 12) ?: 12;
        state.meridiem = value >= 12 ? DateTimeFieldsState::Meridiem::PM : DateTimeFieldsState::Meridiem::AM;
        return;
    }
}

void DateTimeHourFieldElement::setValueAsDate(const DateComponents& date)
{
    int hour = date.hour();

    switch (maximum()) {
    case 11:
        setValueAsInteger(hour % 12);
        return;
    case 12:
        setValueAsInteger((hour % 12) ?: 12);
        return;
    case 23:
        setValueAsInteger(hour);
        return;
    case 24:
        setValueAsInteger(hour + 1);
        return;
    }
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeMeridiemFieldElement);

DateTimeMeridiemFieldElement::DateTimeMeridiemFieldElement(Document& document, FieldOwner& fieldOwner, const Vector<String>& labels)
    : DateTimeSymbolicFieldElement(document, fieldOwner, labels, labels.size() - 1)
{
}

Ref<DateTimeMeridiemFieldElement> DateTimeMeridiemFieldElement::create(Document& document, FieldOwner& fieldOwner, const Vector<String>& labels)
{
    auto element = adoptRef(*new DateTimeMeridiemFieldElement(document, fieldOwner, labels));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditMeridiemField());
    return element;
}

void DateTimeMeridiemFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& state)
{
    if (hasValue())
        state.meridiem = valueAsInteger() ? DateTimeFieldsState::Meridiem::PM : DateTimeFieldsState::Meridiem::AM;
}

void DateTimeMeridiemFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.hour() >= 12 ? 1 : 0);
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeMillisecondFieldElement);

DateTimeMillisecondFieldElement::DateTimeMillisecondFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, Range(0, 999), 0)
{
}

Ref<DateTimeMillisecondFieldElement> DateTimeMillisecondFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    auto element = adoptRef(*new DateTimeMillisecondFieldElement(document, fieldOwner));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditMillisecondField());
    return element;
}

void DateTimeMillisecondFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& state)
{
    if (hasValue())
        state.millisecond = valueAsInteger();
}

void DateTimeMillisecondFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.millisecond());
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeMinuteFieldElement);

DateTimeMinuteFieldElement::DateTimeMinuteFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, Range(0, 59), 30)
{
}

Ref<DateTimeMinuteFieldElement> DateTimeMinuteFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    auto element = adoptRef(*new DateTimeMinuteFieldElement(document, fieldOwner));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditMinuteField());
    return element;
}

void DateTimeMinuteFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& state)
{
    if (hasValue())
        state.minute = valueAsInteger();
}

void DateTimeMinuteFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.minute());
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeMonthFieldElement);

DateTimeMonthFieldElement::DateTimeMonthFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, Range(1, 12), fieldOwner.placeholderDate().month() + 1)
{
}

Ref<DateTimeMonthFieldElement> DateTimeMonthFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    auto element = adoptRef(*new DateTimeMonthFieldElement(document, fieldOwner));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditMonthField());
    element->setAttributeWithoutSynchronization(HTMLNames::aria_labelAttr, AtomString { AXDateFieldMonthText() });
    element->setAttributeWithoutSynchronization(HTMLNames::roleAttr, AtomString { "spinbutton"_s });
    return element;
}

void DateTimeMonthFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& state)
{
    if (hasValue())
        state.month = valueAsInteger();
}

void DateTimeMonthFieldElement::setValueAsDate(const DateComponents& date)
{
    // DateComponents represents months from 0 (January) to 11 (December).
    setValueAsInteger(date.month() + 1);
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeSecondFieldElement);

DateTimeSecondFieldElement::DateTimeSecondFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, Range(0, 59), 0)
{
}

Ref<DateTimeSecondFieldElement> DateTimeSecondFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    auto element = adoptRef(*new DateTimeSecondFieldElement(document, fieldOwner));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditSecondField());
    return element;
}

void DateTimeSecondFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& state)
{
    if (hasValue())
        state.second = valueAsInteger();
}

void DateTimeSecondFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.second());
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeSymbolicMonthFieldElement);

DateTimeSymbolicMonthFieldElement::DateTimeSymbolicMonthFieldElement(Document& document, FieldOwner& fieldOwner, const Vector<String>& labels)
    : DateTimeSymbolicFieldElement(document, fieldOwner, labels, fieldOwner.placeholderDate().month())
{
}

Ref<DateTimeSymbolicMonthFieldElement> DateTimeSymbolicMonthFieldElement::create(Document& document, FieldOwner& fieldOwner, const Vector<String>& labels)
{
    auto element = adoptRef(*new DateTimeSymbolicMonthFieldElement(document, fieldOwner, labels));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditMonthField());
    element->setAttributeWithoutSynchronization(HTMLNames::aria_labelAttr, AtomString { AXDateFieldMonthText() });
    element->setAttributeWithoutSynchronization(HTMLNames::roleAttr, AtomString { "spinbutton"_s });
    return element;
}

void DateTimeSymbolicMonthFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& state)
{
    if (hasValue())
        state.month = valueAsInteger() + 1;
}

void DateTimeSymbolicMonthFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.month());
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeYearFieldElement);

DateTimeYearFieldElement::DateTimeYearFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, Range(DateComponents::minimumYear(), DateComponents::maximumYear()), fieldOwner.placeholderDate().year())
{
}

Ref<DateTimeYearFieldElement> DateTimeYearFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    auto element = adoptRef(*new DateTimeYearFieldElement(document, fieldOwner));
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { element };
    element->setUserAgentPart(UserAgentParts::webkitDatetimeEditYearField());
    element->setAttributeWithoutSynchronization(HTMLNames::aria_labelAttr, AtomString { AXDateFieldYearText() });
    element->setAttributeWithoutSynchronization(HTMLNames::roleAttr, AtomString { "spinbutton"_s });
    return element;
}

void DateTimeYearFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& state)
{
    if (hasValue())
        state.year = valueAsInteger();
}

void DateTimeYearFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.fullYear());
}

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
