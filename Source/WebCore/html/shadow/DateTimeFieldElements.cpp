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
#include <wtf/CurrentTime.h>
#include <wtf/DateMath.h>

namespace WebCore {

DateTimeAMPMFieldElement::DateTimeAMPMFieldElement(Document* document, FieldOwner& fieldOwner, const Vector<String>& ampmLabels)
    : DateTimeSymbolicFieldElement(document, fieldOwner, ampmLabels)
{
}

PassRefPtr<DateTimeAMPMFieldElement> DateTimeAMPMFieldElement::create(Document* document, FieldOwner& fieldOwner, const Vector<String>& ampmLabels)
{
    DEFINE_STATIC_LOCAL(AtomicString, ampmPsuedoId, ("-webkit-datetime-edit-ampm-field", AtomicString::ConstructFromLiteral));
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

void DateTimeAMPMFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (dateTimeFieldsState.hasAMPM())
        setValueAsInteger(dateTimeFieldsState.ampm());
    else
        setEmptyValue();
}

// ----------------------------

DateTimeDayFieldElement::DateTimeDayFieldElement(Document* document, FieldOwner& fieldOwner, const String& placeholder)
    : DateTimeNumericFieldElement(document, fieldOwner, 1, 31, placeholder)
{
}

PassRefPtr<DateTimeDayFieldElement> DateTimeDayFieldElement::create(Document* document, FieldOwner& fieldOwner, const String& placeholder)
{
    DEFINE_STATIC_LOCAL(AtomicString, dayPsuedoId, ("-webkit-datetime-edit-day-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeDayFieldElement> field = adoptRef(new DateTimeDayFieldElement(document, fieldOwner, placeholder.isEmpty() ? ASCIILiteral("--") : placeholder));
    field->initialize(dayPsuedoId, AXDayOfMonthFieldText());
    return field.release();
}

void DateTimeDayFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    dateTimeFieldsState.setDayOfMonth(hasValue() ? valueAsInteger() : DateTimeFieldsState::emptyValue);
}

void DateTimeDayFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.monthDay());
}

void DateTimeDayFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (!dateTimeFieldsState.hasDayOfMonth()) {
        setEmptyValue();
        return;
    }

    const unsigned value = dateTimeFieldsState.dayOfMonth();
    if (range().isInRange(static_cast<int>(value))) {
        setValueAsInteger(value);
        return;
    }

    setEmptyValue();
}

// ----------------------------

DateTimeHourFieldElement::DateTimeHourFieldElement(Document* document, FieldOwner& fieldOwner, int minimum, int maximum, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeNumericFieldElement(document, fieldOwner, minimum, maximum, "--", parameters)
    , m_alignment(maximum + maximum % 2)
{
    ASSERT((!minimum && (maximum == 11 || maximum == 23)) || (minimum == 1 && (maximum == 12 || maximum == 24)));
}

PassRefPtr<DateTimeHourFieldElement> DateTimeHourFieldElement::create(Document* document, FieldOwner& fieldOwner, int minimum, int maximum, const DateTimeNumericFieldElement::Parameters& parameters)
{
    DEFINE_STATIC_LOCAL(AtomicString, hourPsuedoId, ("-webkit-datetime-edit-hour-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeHourFieldElement> field = adoptRef(new DateTimeHourFieldElement(document, fieldOwner, minimum, maximum, parameters));
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

void DateTimeHourFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (!dateTimeFieldsState.hasHour()) {
        setEmptyValue();
        return;
    }

    const int hour12 = dateTimeFieldsState.hour();

    if (hour12 < 1 || hour12 > 12) {
        setEmptyValue();
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

DateTimeMillisecondFieldElement::DateTimeMillisecondFieldElement(Document* document, FieldOwner& fieldOwner, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeNumericFieldElement(document, fieldOwner, 0, 999, "---", parameters)
{
}

PassRefPtr<DateTimeMillisecondFieldElement> DateTimeMillisecondFieldElement::create(Document* document, FieldOwner& fieldOwner, const DateTimeNumericFieldElement::Parameters& parameters)
{
    DEFINE_STATIC_LOCAL(AtomicString, millisecondPsuedoId, ("-webkit-datetime-edit-millisecond-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeMillisecondFieldElement> field = adoptRef(new DateTimeMillisecondFieldElement(document, fieldOwner, parameters));
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

void DateTimeMillisecondFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (!dateTimeFieldsState.hasMillisecond()) {
        setEmptyValue();
        return;
    }

    const unsigned value = dateTimeFieldsState.millisecond();
    if (value > static_cast<unsigned>(maximum())) {
        setEmptyValue();
        return;
    }

    setValueAsInteger(value);
}

// ----------------------------

DateTimeMinuteFieldElement::DateTimeMinuteFieldElement(Document* document, FieldOwner& fieldOwner, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeNumericFieldElement(document, fieldOwner, 0, 59, "--", parameters)
{
}

PassRefPtr<DateTimeMinuteFieldElement> DateTimeMinuteFieldElement::create(Document* document, FieldOwner& fieldOwner, const DateTimeNumericFieldElement::Parameters& parameters)
{
    DEFINE_STATIC_LOCAL(AtomicString, minutePsuedoId, ("-webkit-datetime-edit-minute-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeMinuteFieldElement> field = adoptRef(new DateTimeMinuteFieldElement(document, fieldOwner, parameters));
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

void DateTimeMinuteFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (!dateTimeFieldsState.hasMinute()) {
        setEmptyValue();
        return;
    }

    const unsigned value = dateTimeFieldsState.minute();
    if (value > static_cast<unsigned>(maximum())) {
        setEmptyValue();
        return;
    }

    setValueAsInteger(value);
}

// ----------------------------

DateTimeMonthFieldElement::DateTimeMonthFieldElement(Document* document, FieldOwner& fieldOwner, const String& placeholder)
    : DateTimeNumericFieldElement(document, fieldOwner, 1, 12, placeholder)
{
}

PassRefPtr<DateTimeMonthFieldElement> DateTimeMonthFieldElement::create(Document* document, FieldOwner& fieldOwner, const String& placeholder)
{
    DEFINE_STATIC_LOCAL(AtomicString, monthPsuedoId, ("-webkit-datetime-edit-month-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeMonthFieldElement> field = adoptRef(new DateTimeMonthFieldElement(document, fieldOwner, placeholder.isEmpty() ? ASCIILiteral("--") : placeholder));
    field->initialize(monthPsuedoId, AXMonthFieldText());
    return field.release();
}

void DateTimeMonthFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    dateTimeFieldsState.setMonth(hasValue() ? valueAsInteger() : DateTimeFieldsState::emptyValue);
}

void DateTimeMonthFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.month() + 1);
}

void DateTimeMonthFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (!dateTimeFieldsState.hasMonth()) {
        setEmptyValue();
        return;
    }

    const unsigned value = dateTimeFieldsState.month();
    if (range().isInRange(static_cast<int>(value))) {
        setValueAsInteger(value);
        return;
    }

    setEmptyValue();
}

// ----------------------------

DateTimeSecondFieldElement::DateTimeSecondFieldElement(Document* document, FieldOwner& fieldOwner, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeNumericFieldElement(document, fieldOwner, 0, 59, "--", parameters)
{
}

PassRefPtr<DateTimeSecondFieldElement> DateTimeSecondFieldElement::create(Document* document, FieldOwner& fieldOwner, const DateTimeNumericFieldElement::Parameters& parameters)
{
    DEFINE_STATIC_LOCAL(AtomicString, secondPsuedoId, ("-webkit-datetime-edit-second-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeSecondFieldElement> field = adoptRef(new DateTimeSecondFieldElement(document, fieldOwner, parameters));
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

void DateTimeSecondFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (!dateTimeFieldsState.hasSecond()) {
        setEmptyValue();
        return;
    }

    const unsigned value = dateTimeFieldsState.second();
    if (value > static_cast<unsigned>(maximum())) {
        setEmptyValue();
        return;
    }

    setValueAsInteger(value);
}

// ----------------------------

DateTimeSymbolicMonthFieldElement::DateTimeSymbolicMonthFieldElement(Document* document, FieldOwner& fieldOwner, const Vector<String>& labels)
    : DateTimeSymbolicFieldElement(document, fieldOwner, labels)
{
}

PassRefPtr<DateTimeSymbolicMonthFieldElement> DateTimeSymbolicMonthFieldElement::create(Document* document, FieldOwner& fieldOwner, const Vector<String>& labels)
{
    DEFINE_STATIC_LOCAL(AtomicString, monthPsuedoId, ("-webkit-datetime-edit-month-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeSymbolicMonthFieldElement> field = adoptRef(new DateTimeSymbolicMonthFieldElement(document, fieldOwner, labels));
    field->initialize(monthPsuedoId, AXMonthFieldText());
    return field.release();
}

void DateTimeSymbolicMonthFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    if (!hasValue())
        dateTimeFieldsState.setMonth(DateTimeFieldsState::emptyValue);
    ASSERT(valueAsInteger() < static_cast<int>(symbolsSize()));
    dateTimeFieldsState.setMonth(valueAsInteger() + 1);
}

void DateTimeSymbolicMonthFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.month());
}

void DateTimeSymbolicMonthFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (!dateTimeFieldsState.hasMonth()) {
        setEmptyValue();
        return;
    }

    const unsigned value = dateTimeFieldsState.month() - 1;
    if (value >= symbolsSize()) {
        setEmptyValue();
        return;
    }

    setValueAsInteger(value);
}

// ----------------------------

DateTimeWeekFieldElement::DateTimeWeekFieldElement(Document* document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, DateComponents::minimumWeekNumber, DateComponents::maximumWeekNumber, "--")
{
}

PassRefPtr<DateTimeWeekFieldElement> DateTimeWeekFieldElement::create(Document* document, FieldOwner& fieldOwner)
{
    DEFINE_STATIC_LOCAL(AtomicString, weekPsuedoId, ("-webkit-datetime-edit-week-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeWeekFieldElement> field = adoptRef(new DateTimeWeekFieldElement(document, fieldOwner));
    field->initialize(weekPsuedoId, AXWeekOfYearFieldText());
    return field.release();
}

void DateTimeWeekFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    dateTimeFieldsState.setWeekOfYear(hasValue() ? valueAsInteger() : DateTimeFieldsState::emptyValue);
}

void DateTimeWeekFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.week());
}

void DateTimeWeekFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (!dateTimeFieldsState.hasWeekOfYear()) {
        setEmptyValue();
        return;
    }

    const unsigned value = dateTimeFieldsState.weekOfYear();
    if (range().isInRange(static_cast<int>(value))) {
        setValueAsInteger(value);
        return;
    }

    setEmptyValue();
}

// ----------------------------

DateTimeYearFieldElement::DateTimeYearFieldElement(Document* document, FieldOwner& fieldOwner, const DateTimeYearFieldElement::Parameters& parameters)
    : DateTimeNumericFieldElement(document, fieldOwner, parameters.minimumYear, parameters.maximumYear, parameters.placeholder.isEmpty() ? ASCIILiteral("----") : parameters.placeholder)
    , m_minIsSpecified(parameters.minIsSpecified)
    , m_maxIsSpecified(parameters.maxIsSpecified)
{
    ASSERT(parameters.minimumYear >= DateComponents::minimumYear());
    ASSERT(parameters.maximumYear <= DateComponents::maximumYear());
}

PassRefPtr<DateTimeYearFieldElement> DateTimeYearFieldElement::create(Document* document, FieldOwner& fieldOwner, const DateTimeYearFieldElement::Parameters& parameters)
{
    DEFINE_STATIC_LOCAL(AtomicString, yearPsuedoId, ("-webkit-datetime-edit-year-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeYearFieldElement> field = adoptRef(new DateTimeYearFieldElement(document, fieldOwner, parameters));
    field->initialize(yearPsuedoId, AXYearFieldText());
    return field.release();
}

int DateTimeYearFieldElement::clampValueForHardLimits(int value) const
{
    return Range(DateComponents::minimumYear(), DateComponents::maximumYear()).clampValue(value);
}

static int currentFullYear()
{
    double current = currentTimeMS();
    double utcOffset = calculateUTCOffset();
    double dstOffset = calculateDSTOffset(current, utcOffset);
    int offset = static_cast<int>((utcOffset + dstOffset) / msPerMinute);
    current += offset * msPerMinute;

    DateComponents date;
    date.setMillisecondsSinceEpochForMonth(current);
    return date.fullYear();
}

int DateTimeYearFieldElement::defaultValueForStepDown() const
{
    return m_maxIsSpecified ? DateTimeNumericFieldElement::defaultValueForStepDown() : currentFullYear();
}

int DateTimeYearFieldElement::defaultValueForStepUp() const
{
    return m_minIsSpecified ? DateTimeNumericFieldElement::defaultValueForStepUp() : currentFullYear();
}

void DateTimeYearFieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    dateTimeFieldsState.setYear(hasValue() ? valueAsInteger() : DateTimeFieldsState::emptyValue);
}

void DateTimeYearFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.fullYear());
}

void DateTimeYearFieldElement::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
{
    if (!dateTimeFieldsState.hasYear()) {
        setEmptyValue();
        return;
    }

    const unsigned value = dateTimeFieldsState.year();
    if (range().isInRange(static_cast<int>(value))) {
        setValueAsInteger(value);
        return;
    }

    setEmptyValue();
}

} // namespace WebCore

#endif
