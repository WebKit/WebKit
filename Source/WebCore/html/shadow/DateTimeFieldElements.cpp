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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeDayFieldElement);

DateTimeDayFieldElement::DateTimeDayFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, "--"_s)
{
    static MainThreadNeverDestroyed<const AtomString> dayPseudoId("-webkit-datetime-edit-day-field", AtomString::ConstructFromLiteral);
    initialize(dayPseudoId);
}

Ref<DateTimeDayFieldElement> DateTimeDayFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    return adoptRef(*new DateTimeDayFieldElement(document, fieldOwner));
}

void DateTimeDayFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.monthDay());
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeMonthFieldElement);

DateTimeMonthFieldElement::DateTimeMonthFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, "--"_s)
{
    static MainThreadNeverDestroyed<const AtomString> monthPseudoId("-webkit-datetime-edit-month-field", AtomString::ConstructFromLiteral);
    initialize(monthPseudoId);
}

Ref<DateTimeMonthFieldElement> DateTimeMonthFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    return adoptRef(*new DateTimeMonthFieldElement(document, fieldOwner));
}

void DateTimeMonthFieldElement::setValueAsDate(const DateComponents& date)
{
    // DateComponents represents months from 0 (January) to 11 (December).
    setValueAsInteger(date.month() + 1);
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

void DateTimeSymbolicMonthFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.month());
}

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeYearFieldElement);

DateTimeYearFieldElement::DateTimeYearFieldElement(Document& document, FieldOwner& fieldOwner)
    : DateTimeNumericFieldElement(document, fieldOwner, "----"_s)
{
    static MainThreadNeverDestroyed<const AtomString> yearPseudoId("-webkit-datetime-edit-year-field", AtomString::ConstructFromLiteral);
    initialize(yearPseudoId);
}

Ref<DateTimeYearFieldElement> DateTimeYearFieldElement::create(Document& document, FieldOwner& fieldOwner)
{
    return adoptRef(*new DateTimeYearFieldElement(document, fieldOwner));
}

void DateTimeYearFieldElement::setValueAsDate(const DateComponents& date)
{
    setValueAsInteger(date.fullYear());
}

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
