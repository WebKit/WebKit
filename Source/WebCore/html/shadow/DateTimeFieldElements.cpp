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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeDayFieldElement);

DateTimeDayFieldElement::DateTimeDayFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, Range(1, 31), "--"_s)
{
    static MainThreadNeverDestroyed<const AtomString> dayPseudoId("-webkit-datetime-edit-day-field", AtomString::ConstructFromLiteral);
    initialize(dayPseudoId);
}

Ref<DateTimeDayFieldElement> DateTimeDayFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    return adoptRef(*new DateTimeDayFieldElement(document, fieldOwner));
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
    : DateTimeNumericFieldElement(document, fieldOwner, Range(minimum, maximum), "--"_s)
{
    static MainThreadNeverDestroyed<const AtomString> hourPseudoId("-webkit-datetime-edit-hour-field", AtomString::ConstructFromLiteral);
    initialize(hourPseudoId);
}

Ref<DateTimeHourFieldElement> DateTimeHourFieldElement::create(Document& document, FieldOwner& fieldOwner, int minimum, int maximum)
{
    return adoptRef(*new DateTimeHourFieldElement(document, fieldOwner, minimum, maximum));
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
    : DateTimeSymbolicFieldElement(document, fieldOwner, labels)
{
    static MainThreadNeverDestroyed<const AtomString> meridiemPseudoId("-webkit-datetime-edit-meridiem-field", AtomString::ConstructFromLiteral);
    initialize(meridiemPseudoId);
}

Ref<DateTimeMeridiemFieldElement> DateTimeMeridiemFieldElement::create(Document& document, FieldOwner& fieldOwner, const Vector<String>& labels)
{
    return adoptRef(*new DateTimeMeridiemFieldElement(document, fieldOwner, labels));
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
    : DateTimeNumericFieldElement(document, fieldOwner, Range(0, 999), "--"_s)
{
    static MainThreadNeverDestroyed<const AtomString> millisecondPseudoId("-webkit-datetime-edit-millisecond-field", AtomString::ConstructFromLiteral);
    initialize(millisecondPseudoId);
}

Ref<DateTimeMillisecondFieldElement> DateTimeMillisecondFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    return adoptRef(*new DateTimeMillisecondFieldElement(document, fieldOwner));
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
    : DateTimeNumericFieldElement(document, fieldOwner, Range(0, 59), "--"_s)
{
    static MainThreadNeverDestroyed<const AtomString> minutePseudoId("-webkit-datetime-edit-minute-field", AtomString::ConstructFromLiteral);
    initialize(minutePseudoId);
}

Ref<DateTimeMinuteFieldElement> DateTimeMinuteFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    return adoptRef(*new DateTimeMinuteFieldElement(document, fieldOwner));
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
    : DateTimeNumericFieldElement(document, fieldOwner, Range(1, 12), "--"_s)
{
    static MainThreadNeverDestroyed<const AtomString> monthPseudoId("-webkit-datetime-edit-month-field", AtomString::ConstructFromLiteral);
    initialize(monthPseudoId);
}

Ref<DateTimeMonthFieldElement> DateTimeMonthFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    return adoptRef(*new DateTimeMonthFieldElement(document, fieldOwner));
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
    : DateTimeNumericFieldElement(document, fieldOwner, Range(0, 59), "--"_s)
{
    static MainThreadNeverDestroyed<const AtomString> secondPseudoId("-webkit-datetime-edit-second-field", AtomString::ConstructFromLiteral);
    initialize(secondPseudoId);
}

Ref<DateTimeSecondFieldElement> DateTimeSecondFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    return adoptRef(*new DateTimeSecondFieldElement(document, fieldOwner));
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
    : DateTimeSymbolicFieldElement(document, fieldOwner, labels)
{
    static MainThreadNeverDestroyed<const AtomString> monthPseudoId("-webkit-datetime-edit-month-field", AtomString::ConstructFromLiteral);
    initialize(monthPseudoId);
}

Ref<DateTimeSymbolicMonthFieldElement> DateTimeSymbolicMonthFieldElement::create(Document& document, FieldOwner& fieldOwner, const Vector<String>& labels)
{
    return adoptRef(*new DateTimeSymbolicMonthFieldElement(document, fieldOwner, labels));
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
    : DateTimeNumericFieldElement(document, fieldOwner, Range(DateComponents::minimumYear(), DateComponents::maximumYear()), "----"_s)
{
    static MainThreadNeverDestroyed<const AtomString> yearPseudoId("-webkit-datetime-edit-year-field", AtomString::ConstructFromLiteral);
    initialize(yearPseudoId);
}

Ref<DateTimeYearFieldElement> DateTimeYearFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    return adoptRef(*new DateTimeYearFieldElement(document, fieldOwner));
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
