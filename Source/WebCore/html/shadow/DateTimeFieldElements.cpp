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
#include "DateTimeFieldElements.h"

#include "DateComponents.h"
#include <wtf/DateMath.h>

namespace WebCore {

DateTimeAMPMFieldElement::DateTimeAMPMFieldElement(Document* document, FieldEventHandler& fieldEventHandler, const Vector<String>& ampmLabels)
    : DateTimeSymbolicFieldElement(document, fieldEventHandler, ampmLabels)
{
}

PassRefPtr<DateTimeAMPMFieldElement> DateTimeAMPMFieldElement::create(Document* document, FieldEventHandler& fieldEventHandler, const Vector<String>& ampmLabels)
{
    DEFINE_STATIC_LOCAL(AtomicString, ampmPsuedoId, ("-webkit-datetime-edit-ampm-field"));
    RefPtr<DateTimeAMPMFieldElement> field = adoptRef(new DateTimeAMPMFieldElement(document, fieldEventHandler, ampmLabels));
    field->initialize(ampmPsuedoId);
    return field.release();
}

void DateTimeAMPMFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.hour() >= 12 ? 1 : 0);
}

double DateTimeAMPMFieldElement::unitInMillisecond() const
{
    return msPerHour * 12;
}

// ----------------------------

DateTimeHourFieldElement::DateTimeHourFieldElement(Document* document, FieldEventHandler& fieldEventHandler, int minimum, int maximum)
    : DateTimeNumericFieldElement(document, fieldEventHandler, minimum, maximum)
    , m_alignment(maximum + maximum % 2)
{
    ASSERT((!minimum && (maximum == 11 || maximum == 23)) || (minimum == 1 && (maximum == 12 || maximum == 24)));
}

PassRefPtr<DateTimeHourFieldElement> DateTimeHourFieldElement::create(Document* document, FieldEventHandler& fieldEventHandler, int minimum, int maximum)
{
    DEFINE_STATIC_LOCAL(AtomicString, hourPsuedoId, ("-webkit-datetime-edit-hour-field"));
    RefPtr<DateTimeHourFieldElement> field = adoptRef(new DateTimeHourFieldElement(document, fieldEventHandler, minimum, maximum));
    field->initialize(hourPsuedoId);
    return field.release();
}

void DateTimeHourFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.hour());
}

void DateTimeHourFieldElement::setValueAsInteger(int valueAsHour23, EventBehavior eventBehavior)
{
    const int value = Range(0, 23).clampValue(valueAsHour23) % m_alignment;
    DateTimeNumericFieldElement::setValueAsInteger(range().minimum && !value ? m_alignment : value, eventBehavior);
}

double DateTimeHourFieldElement::unitInMillisecond() const
{
    return msPerHour;
}

int DateTimeHourFieldElement::valueAsInteger() const
{
    return hasValue() ? DateTimeNumericFieldElement::valueAsInteger() % m_alignment : -1;
}

// ----------------------------

DateTimeMillisecondFieldElement::DateTimeMillisecondFieldElement(Document* document, FieldEventHandler& fieldEventHandler)
    : DateTimeNumericFieldElement(document, fieldEventHandler, 0, 999)
{
}

PassRefPtr<DateTimeMillisecondFieldElement> DateTimeMillisecondFieldElement::create(Document* document, FieldEventHandler& fieldEventHandler)
{
    DEFINE_STATIC_LOCAL(AtomicString, millisecondPsuedoId, ("-webkit-datetime-edit-millisecond-field"));
    RefPtr<DateTimeMillisecondFieldElement> field = adoptRef(new DateTimeMillisecondFieldElement(document, fieldEventHandler));
    field->initialize(millisecondPsuedoId);
    return field.release();
}

void DateTimeMillisecondFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.millisecond());
}

double DateTimeMillisecondFieldElement::unitInMillisecond() const
{
    return 1;
}

// ----------------------------

DateTimeMinuteFieldElement::DateTimeMinuteFieldElement(Document* document, FieldEventHandler& fieldEventHandler)
    : DateTimeNumericFieldElement(document, fieldEventHandler, 0, 59)
{
}

PassRefPtr<DateTimeMinuteFieldElement> DateTimeMinuteFieldElement::create(Document* document, FieldEventHandler& fieldEventHandler)
{
    DEFINE_STATIC_LOCAL(AtomicString, minutePsuedoId, ("-webkit-datetime-edit-minute-field"));
    RefPtr<DateTimeMinuteFieldElement> field = adoptRef(new DateTimeMinuteFieldElement(document, fieldEventHandler));
    field->initialize(minutePsuedoId);
    return field.release();
}

void DateTimeMinuteFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.minute());
}

double DateTimeMinuteFieldElement::unitInMillisecond() const
{
    return msPerMinute;
}

// ----------------------------

DateTimeSecondFieldElement::DateTimeSecondFieldElement(Document* document, FieldEventHandler& fieldEventHandler)
    : DateTimeNumericFieldElement(document, fieldEventHandler, 0, 59)
{
}

PassRefPtr<DateTimeSecondFieldElement> DateTimeSecondFieldElement::create(Document* document, FieldEventHandler& fieldEventHandler)
{
    DEFINE_STATIC_LOCAL(AtomicString, secondPsuedoId, ("-webkit-datetime-edit-second-field"));
    RefPtr<DateTimeSecondFieldElement> field = adoptRef(new DateTimeSecondFieldElement(document, fieldEventHandler));
    field->initialize(secondPsuedoId);
    return field.release();
}

void DateTimeSecondFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.second());
}

double DateTimeSecondFieldElement::unitInMillisecond() const
{
    return msPerSecond;
}

} // namespace WebCore

#endif
