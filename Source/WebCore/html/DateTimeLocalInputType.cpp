/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DateTimeLocalInputType.h"

#include "DateComponents.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputTypeNames.h"
#include <wtf/PassOwnPtr.h>

#if ENABLE(INPUT_TYPE_DATETIMELOCAL)

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeFieldsState.h"
#include "LocalizedStrings.h"
#include "PlatformLocale.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>
#endif

namespace WebCore {

using namespace HTMLNames;

static const int dateTimeLocalDefaultStep = 60;
static const int dateTimeLocalDefaultStepBase = 0;
static const int dateTimeLocalStepScaleFactor = 1000;

PassOwnPtr<InputType> DateTimeLocalInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new DateTimeLocalInputType(element));
}

const AtomicString& DateTimeLocalInputType::formControlType() const
{
    return InputTypeNames::datetimelocal();
}

DateComponents::Type DateTimeLocalInputType::dateType() const
{
    return DateComponents::DateTimeLocal;
}

double DateTimeLocalInputType::valueAsDate() const
{
    // valueAsDate doesn't work for the datetime-local type according to the standard.
    return DateComponents::invalidMilliseconds();
}

void DateTimeLocalInputType::setValueAsDate(double value, ExceptionCode& ec) const
{
    // valueAsDate doesn't work for the datetime-local type according to the standard.
    InputType::setValueAsDate(value, ec);
}

StepRange DateTimeLocalInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    DEFINE_STATIC_LOCAL(const StepRange::StepDescription, stepDescription, (dateTimeLocalDefaultStep, dateTimeLocalDefaultStepBase, dateTimeLocalStepScaleFactor, StepRange::ScaledStepValueShouldBeInteger));

    const Decimal stepBase = parseToNumber(element()->fastGetAttribute(minAttr), 0);
    const Decimal minimum = parseToNumber(element()->fastGetAttribute(minAttr), Decimal::fromDouble(DateComponents::minimumDateTime()));
    const Decimal maximum = parseToNumber(element()->fastGetAttribute(maxAttr), Decimal::fromDouble(DateComponents::maximumDateTime()));
    const Decimal step = StepRange::parseStep(anyStepHandling, stepDescription, element()->fastGetAttribute(stepAttr));
    return StepRange(stepBase, minimum, maximum, step, stepDescription);
}

bool DateTimeLocalInputType::parseToDateComponentsInternal(const UChar* characters, unsigned length, DateComponents* out) const
{
    ASSERT(out);
    unsigned end;
    return out->parseDateTimeLocal(characters, length, 0, end) && end == length;
}

bool DateTimeLocalInputType::setMillisecondToDateComponents(double value, DateComponents* date) const
{
    ASSERT(date);
    return date->setMillisecondsSinceEpochForDateTimeLocal(value);
}

bool DateTimeLocalInputType::isDateTimeLocalField() const
{
    return true;
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
// FIXME: It is better to share code for DateTimeInputType::formatDateTimeFieldsState()
// and DateTimeInputLocalType::formatDateTimeFieldsState().
String DateTimeLocalInputType::formatDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState) const
{
    if (!dateTimeFieldsState.hasDayOfMonth() || !dateTimeFieldsState.hasMonth() || !dateTimeFieldsState.hasYear()
        || !dateTimeFieldsState.hasHour() || !dateTimeFieldsState.hasMinute() || !dateTimeFieldsState.hasAMPM())
        return emptyString();

    if (dateTimeFieldsState.hasMillisecond() && dateTimeFieldsState.millisecond()) {
        return String::format("%04u-%02u-%02uT%02u:%02u:%02u.%03u",
            dateTimeFieldsState.year(),
            dateTimeFieldsState.month(),
            dateTimeFieldsState.dayOfMonth(),
            dateTimeFieldsState.hour23(),
            dateTimeFieldsState.minute(),
            dateTimeFieldsState.hasSecond() ? dateTimeFieldsState.second() : 0,
            dateTimeFieldsState.millisecond());
    }

    if (dateTimeFieldsState.hasSecond() && dateTimeFieldsState.second()) {
        return String::format("%04u-%02u-%02uT%02u:%02u:%02u",
            dateTimeFieldsState.year(),
            dateTimeFieldsState.month(),
            dateTimeFieldsState.dayOfMonth(),
            dateTimeFieldsState.hour23(),
            dateTimeFieldsState.minute(),
            dateTimeFieldsState.second());
    }

    return String::format("%04u-%02u-%02uT%02u:%02u",
        dateTimeFieldsState.year(),
        dateTimeFieldsState.month(),
        dateTimeFieldsState.dayOfMonth(),
        dateTimeFieldsState.hour23(),
        dateTimeFieldsState.minute());
}

void DateTimeLocalInputType::setupLayoutParameters(DateTimeEditElement::LayoutParameters& layoutParameters, const DateComponents& date) const
{
    if (shouldHaveSecondField(date)) {
        layoutParameters.dateTimeFormat = layoutParameters.locale.dateTimeFormatWithSeconds();
        layoutParameters.fallbackDateTimeFormat = "dd/MM/yyyy HH:mm:ss";
    } else {
        layoutParameters.dateTimeFormat = layoutParameters.locale.dateTimeFormatWithoutSeconds();
        layoutParameters.fallbackDateTimeFormat = "dd/MM/yyyy HH:mm";
    }
    if (!parseToDateComponents(element()->fastGetAttribute(minAttr), &layoutParameters.minimum))
        layoutParameters.minimum = DateComponents();
    if (!parseToDateComponents(element()->fastGetAttribute(maxAttr), &layoutParameters.maximum))
        layoutParameters.maximum = DateComponents();
    layoutParameters.placeholderForDay = placeholderForDayOfMonthField();
    layoutParameters.placeholderForMonth = placeholderForMonthField();
    layoutParameters.placeholderForYear = placeholderForYearField();
}
#endif

} // namespace WebCore

#endif
