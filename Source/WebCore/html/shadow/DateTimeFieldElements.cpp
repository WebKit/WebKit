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
#include "DateTimeFieldElements.h"

#include "DateComponents.h"
#include "DateTimeFieldsState.h"
#include "LocalizedStrings.h"

namespace WebCore {

DateTimeAMPMFieldElement::DateTimeAMPMFieldElement(Document* document, FieldOwner& fieldOwner, const Vector<String>& ampmLabels)
    : DateTimeSymbolicFieldElement(document, fieldOwner, ampmLabels)
{
}

PassRefPtr<DateTimeAMPMFieldElement> DateTimeAMPMFieldElement::create(Document* document, FieldOwner& fieldOwner, const Vector<String>& ampmLabels)
{
    DEFINE_STATIC_LOCAL(AtomicString, ampmPsuedoId, ("-webkit-datetime-edit-ampm-field"));
    RefPtr<DateTimeAMPMFieldElement> field = adoptRef(new DateTimeAMPMFieldElement(document, fieldOwner, ampmLabels));
    field->initialize(ampmPsuedoId, AXAMPMFieldText());
    return field.release();
}

void DateTimeAMPMFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    if (hasValue())
        dateTimeFieldsState.setAMPM(valueAsInteger() ? DateTimeFieldsState::AMPMValuePM : DateTimeFieldsState::AMPMValueAM);
    else
        dateTimeFieldsState.setAMPM(DateTimeFieldsState::AMPMValueEmpty);
}

void DateTimeAMPMFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.hour() >= 12 ? 1 : 0);
}

void DateTimeAMPMFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState, const DateComponents& dateForReadOnlyField)
{
    if (dateTimeFieldsState.hasAMPM())
        setValueAsInteger(dateTimeFieldsState.ampm());
    else
        setEmptyValue(dateForReadOnlyField);
}

// ----------------------------

DateTimeHourFieldElement::DateTimeHourFieldElement(Document* document, FieldOwner& fieldOwner, int minimum, int maximum)
    : DateTimeNumericFieldElement(document, fieldOwner, minimum, maximum, "--")
    , m_alignment(maximum + maximum % 2)
{
    ASSERT((!minimum && (maximum == 11 || maximum == 23)) || (minimum == 1 && (maximum == 12 || maximum == 24)));
}

PassRefPtr<DateTimeHourFieldElement> DateTimeHourFieldElement::create(Document* document, FieldOwner& fieldOwner, int minimum, int maximum)
{
    DEFINE_STATIC_LOCAL(AtomicString, hourPsuedoId, ("-webkit-datetime-edit-hour-field"));
    RefPtr<DateTimeHourFieldElement> field = adoptRef(new DateTimeHourFieldElement(document, fieldOwner, minimum, maximum));
    field->initialize(hourPsuedoId, AXHourFieldText());
    return field.release();
}

void DateTimeHourFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    if (!hasValue()) {
        dateTimeFieldsState.setHour(DateTimeFieldsState::emptyValue);
        return;
    }

    const int value = DateTimeNumericFieldElement::valueAsInteger();

    switch (maximum()) {
    case 11:
        dateTimeFieldsState.setHour(value ? value : 12);
        return;
    case 12:
        dateTimeFieldsState.setHour(value);
        return;
    case 23:
        dateTimeFieldsState.setHour(value ? value % 12 : 12);
        dateTimeFieldsState.setAMPM(value >= 12 ? DateTimeFieldsState::AMPMValuePM : DateTimeFieldsState::AMPMValueAM);
        return;
    case 24:
        if (value == 24) {
            dateTimeFieldsState.setHour(12);
            dateTimeFieldsState.setHour(DateTimeFieldsState::AMPMValueAM);
            return;
        }
        dateTimeFieldsState.setHour(value == 12 ? 12 : value % 12);
        dateTimeFieldsState.setAMPM(value >= 12 ? DateTimeFieldsState::AMPMValuePM : DateTimeFieldsState::AMPMValueAM);
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

void DateTimeHourFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.hour());
}

void DateTimeHourFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState, const DateComponents& dateForReadOnlyField)
{
    if (!dateTimeFieldsState.hasHour()) {
        setEmptyValue(dateForReadOnlyField);
        return;
    }

    const int hour12 = dateTimeFieldsState.hour();

    if (hour12 < 1 || hour12 > 12) {
        setEmptyValue(dateForReadOnlyField);
        return;
    }

    switch (maximum()) {
    case 11:
        DateTimeNumericFieldElement::setValueAsInteger(hour12 % 12);
        return;
    case 12:
        DateTimeNumericFieldElement::setValueAsInteger(hour12);
        return;
    case 23:
        if (dateTimeFieldsState.ampm() == DateTimeFieldsState::AMPMValuePM)
            DateTimeNumericFieldElement::setValueAsInteger((hour12 + 12) % 24);
        else
            DateTimeNumericFieldElement::setValueAsInteger(hour12 % 12);
        return;
    case 24:
        if (dateTimeFieldsState.ampm() == DateTimeFieldsState::AMPMValuePM)
            DateTimeNumericFieldElement::setValueAsInteger(hour12 == 12 ? 12 : hour12 + 12);
        else
            DateTimeNumericFieldElement::setValueAsInteger(hour12);
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

void DateTimeHourFieldElement::setValueAsInteger(int valueAsHour23, EventBehavior eventBehavior)
{
    const int value = Range(0, 23).clampValue(valueAsHour23) % m_alignment;
    DateTimeNumericFieldElement::setValueAsInteger(range().minimum && !value ? m_alignment : value, eventBehavior);
}

int DateTimeHourFieldElement::valueAsInteger() const
{
    return hasValue() ? DateTimeNumericFieldElement::valueAsInteger() % m_alignment : -1;
}

// ----------------------------

DateTimeMillisecondFieldElement::DateTimeMillisecondFieldElement(Document* document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, 0, 999, "---")
{
}

PassRefPtr<DateTimeMillisecondFieldElement> DateTimeMillisecondFieldElement::create(Document* document, FieldOwner& fieldOwner)
{
    DEFINE_STATIC_LOCAL(AtomicString, millisecondPsuedoId, ("-webkit-datetime-edit-millisecond-field"));
    RefPtr<DateTimeMillisecondFieldElement> field = adoptRef(new DateTimeMillisecondFieldElement(document, fieldOwner));
    field->initialize(millisecondPsuedoId, AXMillisecondFieldText());
    return field.release();
}

void DateTimeMillisecondFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    dateTimeFieldsState.setMillisecond(hasValue() ? valueAsInteger() : DateTimeFieldsState::emptyValue);
}

void DateTimeMillisecondFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.millisecond());
}

void DateTimeMillisecondFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState, const DateComponents& dateForReadOnlyField)
{
    if (!dateTimeFieldsState.hasMillisecond()) {
        setEmptyValue(dateForReadOnlyField);
        return;
    }

    const unsigned value = dateTimeFieldsState.millisecond();
    if (value > static_cast<unsigned>(maximum())) {
        setEmptyValue(dateForReadOnlyField);
        return;
    }

    setValueAsInteger(value);
}

// ----------------------------

DateTimeMinuteFieldElement::DateTimeMinuteFieldElement(Document* document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, 0, 59, "--")
{
}

PassRefPtr<DateTimeMinuteFieldElement> DateTimeMinuteFieldElement::create(Document* document, FieldOwner& fieldOwner)
{
    DEFINE_STATIC_LOCAL(AtomicString, minutePsuedoId, ("-webkit-datetime-edit-minute-field"));
    RefPtr<DateTimeMinuteFieldElement> field = adoptRef(new DateTimeMinuteFieldElement(document, fieldOwner));
    field->initialize(minutePsuedoId, AXMinuteFieldText());
    return field.release();
}

void DateTimeMinuteFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    dateTimeFieldsState.setMinute(hasValue() ? valueAsInteger() : DateTimeFieldsState::emptyValue);
}

void DateTimeMinuteFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.minute());
}

void DateTimeMinuteFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState, const DateComponents& dateForReadOnlyField)
{
    if (!dateTimeFieldsState.hasMinute()) {
        setEmptyValue(dateForReadOnlyField);
        return;
    }

    const unsigned value = dateTimeFieldsState.minute();
    if (value > static_cast<unsigned>(maximum())) {
        setEmptyValue(dateForReadOnlyField);
        return;
    }

    setValueAsInteger(value);
}

// ----------------------------

DateTimeSecondFieldElement::DateTimeSecondFieldElement(Document* document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, 0, 59, "--")
{
}

PassRefPtr<DateTimeSecondFieldElement> DateTimeSecondFieldElement::create(Document* document, FieldOwner& fieldOwner)
{
    DEFINE_STATIC_LOCAL(AtomicString, secondPsuedoId, ("-webkit-datetime-edit-second-field"));
    RefPtr<DateTimeSecondFieldElement> field = adoptRef(new DateTimeSecondFieldElement(document, fieldOwner));
    field->initialize(secondPsuedoId, AXSecondFieldText());
    return field.release();
}

void DateTimeSecondFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    dateTimeFieldsState.setSecond(hasValue() ? valueAsInteger() : DateTimeFieldsState::emptyValue);
}

void DateTimeSecondFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.second());
}

void DateTimeSecondFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState, const DateComponents& dateForReadOnlyField)
{
    if (!dateTimeFieldsState.hasSecond()) {
        setEmptyValue(dateForReadOnlyField);
        return;
    }

    const unsigned value = dateTimeFieldsState.second();
    if (value > static_cast<unsigned>(maximum())) {
        setEmptyValue(dateForReadOnlyField);
        return;
    }

    setValueAsInteger(value);
}

} // namespace WebCore

#endif
