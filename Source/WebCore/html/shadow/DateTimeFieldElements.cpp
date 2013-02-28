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
    : DateTimeSymbolicFieldElement(document, fieldOwner, ampmLabels, 0, 1)
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

DateTimeDayFieldElement::DateTimeDayFieldElement(Document* document, FieldOwner& fieldOwner, const String& placeholder, int minimum, int maximum)
    : DateTimeNumericFieldElement(document, fieldOwner, minimum, maximum, placeholder)
{
}

PassRefPtr<DateTimeDayFieldElement> DateTimeDayFieldElement::create(Document* document, FieldOwner& fieldOwner, const String& placeholder, int minimum, int maximum)
{
    DEFINE_STATIC_LOCAL(AtomicString, dayPsuedoId, ("-webkit-datetime-edit-day-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeDayFieldElement> field = adoptRef(new DateTimeDayFieldElement(document, fieldOwner, placeholder.isEmpty() ? ASCIILiteral("--") : placeholder, minimum, maximum));
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

int DateTimeDayFieldElement::clampValueForHardLimits(int value) const
{
    return Range(1, 31).clampValue(value);
}

// ----------------------------

DateTimeHourFieldElementBase::DateTimeHourFieldElementBase(Document* document, FieldOwner& fieldOwner, int minimum, int maximum, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeNumericFieldElement(document, fieldOwner, minimum, maximum, "--", parameters)
{
}

void DateTimeHourFieldElementBase::initialize()
{
    DEFINE_STATIC_LOCAL(AtomicString, hourPsuedoId, ("-webkit-datetime-edit-hour-field", AtomicString::ConstructFromLiteral));
    DateTimeNumericFieldElement::initialize(hourPsuedoId, AXHourFieldText());
}

void DateTimeHourFieldElementBase::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.hour());
}

void DateTimeHourFieldElementBase::setValueAsDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState)
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

    if (dateTimeFieldsState.ampm() == DateTimeFieldsState::AMPMValuePM)
        setValueAsInteger((hour12 + 12) % 24);
    else
        setValueAsInteger(hour12 % 12);
}
// ----------------------------

DateTimeHour11FieldElement::DateTimeHour11FieldElement(Document* document, FieldOwner& fieldOwner, int minimum, int maximum, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeHourFieldElementBase(document, fieldOwner, minimum, maximum, parameters)
{
}

PassRefPtr<DateTimeHour11FieldElement> DateTimeHour11FieldElement::create(Document* document, FieldOwner& fieldOwner, int minimumHour23, int maximumHour23, const DateTimeNumericFieldElement::Parameters& parameters)
{
    ASSERT(minimumHour23 >= 0);
    ASSERT(maximumHour23 <= 23);
    ASSERT(minimumHour23 <= maximumHour23);
    int minimum = 0, maximum = 11;
    if (maximumHour23 < 12) {
        minimum = minimumHour23;
        maximum = maximumHour23;
    } else if (minimumHour23 >= 12) {
        minimum = minimumHour23 - 12;
        maximum = maximumHour23 - 12;
    }

    RefPtr<DateTimeHour11FieldElement> field = adoptRef(new DateTimeHour11FieldElement(document, fieldOwner, minimum, maximum, parameters));
    field->initialize();
    return field.release();
}

void DateTimeHour11FieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    if (!hasValue()) {
        dateTimeFieldsState.setHour(DateTimeFieldsState::emptyValue);
        return;
    }
    const int value = valueAsInteger();
    dateTimeFieldsState.setHour(value ? value : 12);
}

void DateTimeHour11FieldElement::setValueAsInteger(int value, EventBehavior eventBehavior)
{
    value = Range(0, 23).clampValue(value) % 12;
    DateTimeNumericFieldElement::setValueAsInteger(value, eventBehavior);
}

int DateTimeHour11FieldElement::clampValueForHardLimits(int value) const
{
    return Range(0, 11).clampValue(value);
}

// ----------------------------

DateTimeHour12FieldElement::DateTimeHour12FieldElement(Document* document, FieldOwner& fieldOwner, int minimum, int maximum, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeHourFieldElementBase(document, fieldOwner, minimum, maximum, parameters)
{
}

PassRefPtr<DateTimeHour12FieldElement> DateTimeHour12FieldElement::create(Document* document, FieldOwner& fieldOwner, int minimumHour23, int maximumHour23, const DateTimeNumericFieldElement::Parameters& parameters)
{
    ASSERT(minimumHour23 >= 0);
    ASSERT(maximumHour23 <= 23);
    ASSERT(minimumHour23 <= maximumHour23);
    int minimum = 1, maximum = 12;
    if (maximumHour23 < 12) {
        minimum = minimumHour23;
        maximum = maximumHour23;
    } else if (minimumHour23 >= 12) {
        minimum = minimumHour23 - 12;
        maximum = maximumHour23 - 12;
    }
    if (!minimum)
        minimum = 12;
    if (!maximum)
        maximum = 12;
    if (minimum > maximum) {
        minimum = 1;
        maximum = 12;
    }
    RefPtr<DateTimeHour12FieldElement> field = adoptRef(new DateTimeHour12FieldElement(document, fieldOwner, minimum, maximum, parameters));
    field->initialize();
    return field.release();
}

void DateTimeHour12FieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    dateTimeFieldsState.setHour(hasValue() ? valueAsInteger() : DateTimeFieldsState::emptyValue);
}

void DateTimeHour12FieldElement::setValueAsInteger(int value, EventBehavior eventBehavior)
{
    value = Range(0, 24).clampValue(value) % 12;
    DateTimeNumericFieldElement::setValueAsInteger(value ? value : 12, eventBehavior);
}

int DateTimeHour12FieldElement::clampValueForHardLimits(int value) const
{
    return Range(1, 12).clampValue(value);
}

// ----------------------------

DateTimeHour23FieldElement::DateTimeHour23FieldElement(Document* document, FieldOwner& fieldOwner, int minimum, int maximum, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeHourFieldElementBase(document, fieldOwner, minimum, maximum, parameters)
{
}

PassRefPtr<DateTimeHour23FieldElement> DateTimeHour23FieldElement::create(Document* document, FieldOwner& fieldOwner, int minimumHour23, int maximumHour23, const DateTimeNumericFieldElement::Parameters& parameters)
{
    ASSERT(minimumHour23 >= 0);
    ASSERT(maximumHour23 <= 23);
    ASSERT(minimumHour23 <= maximumHour23);
    RefPtr<DateTimeHour23FieldElement> field = adoptRef(new DateTimeHour23FieldElement(document, fieldOwner, minimumHour23, maximumHour23, parameters));
    field->initialize();
    return field.release();
}

void DateTimeHour23FieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    if (!hasValue()) {
        dateTimeFieldsState.setHour(DateTimeFieldsState::emptyValue);
        return;
    }

    const int value = valueAsInteger();

    dateTimeFieldsState.setHour(value ? value % 12 : 12);
    dateTimeFieldsState.setAMPM(value >= 12 ? DateTimeFieldsState::AMPMValuePM : DateTimeFieldsState::AMPMValueAM);
}

void DateTimeHour23FieldElement::setValueAsInteger(int value, EventBehavior eventBehavior)
{
    value = Range(0, 23).clampValue(value);
    DateTimeNumericFieldElement::setValueAsInteger(value, eventBehavior);
}

int DateTimeHour23FieldElement::clampValueForHardLimits(int value) const
{
    return Range(0, 23).clampValue(value);
}

// ----------------------------

DateTimeHour24FieldElement::DateTimeHour24FieldElement(Document* document, FieldOwner& fieldOwner, int minimum, int maximum, const DateTimeNumericFieldElement::Parameters& parameters)
    : DateTimeHourFieldElementBase(document, fieldOwner, minimum, maximum, parameters)
{
}

PassRefPtr<DateTimeHour24FieldElement> DateTimeHour24FieldElement::create(Document* document, FieldOwner& fieldOwner, int minimumHour23, int maximumHour23, const DateTimeNumericFieldElement::Parameters& parameters)
{
    ASSERT(minimumHour23 >= 0);
    ASSERT(maximumHour23 <= 23);
    ASSERT(minimumHour23 <= maximumHour23);
    int minimum = minimumHour23 ? minimumHour23 : 24;
    int maximum = maximumHour23 ? maximumHour23 : 24;
    if (minimum > maximum) {
        minimum = 1;
        maximum = 24;
    }

    RefPtr<DateTimeHour24FieldElement> field = adoptRef(new DateTimeHour24FieldElement(document, fieldOwner, minimum, maximum, parameters));
    field->initialize();
    return field.release();
}

void DateTimeHour24FieldElement::populateDateTimeFieldsState(DateTimeFieldsState& dateTimeFieldsState)
{
    if (!hasValue()) {
        dateTimeFieldsState.setHour(DateTimeFieldsState::emptyValue);
        return;
    }

    const int value = valueAsInteger();

    if (value == 24) {
        dateTimeFieldsState.setHour(12);
        dateTimeFieldsState.setAMPM(DateTimeFieldsState::AMPMValueAM);
    } else {
        dateTimeFieldsState.setHour(value == 12 ? 12 : value % 12);
        dateTimeFieldsState.setAMPM(value >= 12 ? DateTimeFieldsState::AMPMValuePM : DateTimeFieldsState::AMPMValueAM);
    }
}

void DateTimeHour24FieldElement::setValueAsInteger(int value, EventBehavior eventBehavior)
{
    value = Range(0, 24).clampValue(value);
    DateTimeNumericFieldElement::setValueAsInteger(value ? value : 24, eventBehavior);
}

int DateTimeHour24FieldElement::clampValueForHardLimits(int value) const
{
    return Range(1, 24).clampValue(value);
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

DateTimeMonthFieldElement::DateTimeMonthFieldElement(Document* document, FieldOwner& fieldOwner, const String& placeholder, int minimum, int maximum)
    : DateTimeNumericFieldElement(document, fieldOwner, minimum, maximum, placeholder)
{
}

PassRefPtr<DateTimeMonthFieldElement> DateTimeMonthFieldElement::create(Document* document, FieldOwner& fieldOwner, const String& placeholder, int minimum, int maximum)
{
    DEFINE_STATIC_LOCAL(AtomicString, monthPsuedoId, ("-webkit-datetime-edit-month-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeMonthFieldElement> field = adoptRef(new DateTimeMonthFieldElement(document, fieldOwner, placeholder.isEmpty() ? ASCIILiteral("--") : placeholder, minimum, maximum));
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

int DateTimeMonthFieldElement::clampValueForHardLimits(int value) const
{
    return Range(1, 12).clampValue(value);
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

DateTimeSymbolicMonthFieldElement::DateTimeSymbolicMonthFieldElement(Document* document, FieldOwner& fieldOwner, const Vector<String>& labels, int minimum, int maximum)
    : DateTimeSymbolicFieldElement(document, fieldOwner, labels, minimum, maximum)
{
}

PassRefPtr<DateTimeSymbolicMonthFieldElement> DateTimeSymbolicMonthFieldElement::create(Document* document, FieldOwner& fieldOwner, const Vector<String>& labels, int minimum, int maximum)
{
    DEFINE_STATIC_LOCAL(AtomicString, monthPsuedoId, ("-webkit-datetime-edit-month-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeSymbolicMonthFieldElement> field = adoptRef(new DateTimeSymbolicMonthFieldElement(document, fieldOwner, labels, minimum, maximum));
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

DateTimeWeekFieldElement::DateTimeWeekFieldElement(Document* document, FieldOwner& fieldOwner, int minimum, int maximum)
    : DateTimeNumericFieldElement(document, fieldOwner, minimum, maximum, "--")
{
}

PassRefPtr<DateTimeWeekFieldElement> DateTimeWeekFieldElement::create(Document* document, FieldOwner& fieldOwner, int minimum, int maximum)
{
    DEFINE_STATIC_LOCAL(AtomicString, weekPsuedoId, ("-webkit-datetime-edit-week-field", AtomicString::ConstructFromLiteral));
    RefPtr<DateTimeWeekFieldElement> field = adoptRef(new DateTimeWeekFieldElement(document, fieldOwner, minimum, maximum));
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

int DateTimeWeekFieldElement::clampValueForHardLimits(int value) const
{
    return Range(DateComponents::minimumWeekNumber, DateComponents::maximumWeekNumber).clampValue(value);
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
