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
#if ENABLE(INPUT_TYPE_DATETIME_INCOMPLETE)
#include "DateTimeInputType.h"

#include "DateComponents.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputTypeNames.h"
#include <wtf/CurrentTime.h>
#include <wtf/PassOwnPtr.h>

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeFieldsState.h"
#include "LocalizedStrings.h"
#include "PlatformLocale.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>
#endif

namespace WebCore {

using namespace HTMLNames;

static const int dateTimeDefaultStep = 60;
static const int dateTimeDefaultStepBase = 0;
static const int dateTimeStepScaleFactor = 1000;

PassOwnPtr<InputType> DateTimeInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new DateTimeInputType(element));
}

void DateTimeInputType::attach()
{
    observeFeatureIfVisible(FeatureObserver::InputTypeDateTime);
}

const AtomicString& DateTimeInputType::formControlType() const
{
    return InputTypeNames::datetime();
}

DateComponents::Type DateTimeInputType::dateType() const
{
    return DateComponents::DateTime;
}

Decimal DateTimeInputType::defaultValueForStepUp() const
{
    return Decimal::fromDouble(currentTimeMS());
}

StepRange DateTimeInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    DEFINE_STATIC_LOCAL(const StepRange::StepDescription, stepDescription, (dateTimeDefaultStep, dateTimeDefaultStepBase, dateTimeStepScaleFactor, StepRange::ScaledStepValueShouldBeInteger));

    const Decimal stepBase = parseToNumber(element()->fastGetAttribute(minAttr), 0);
    const Decimal minimum = parseToNumber(element()->fastGetAttribute(minAttr), Decimal::fromDouble(DateComponents::minimumDateTime()));
    const Decimal maximum = parseToNumber(element()->fastGetAttribute(maxAttr), Decimal::fromDouble(DateComponents::maximumDateTime()));
    const Decimal step = StepRange::parseStep(anyStepHandling, stepDescription, element()->fastGetAttribute(stepAttr));
    return StepRange(stepBase, minimum, maximum, step, stepDescription);
}

bool DateTimeInputType::parseToDateComponentsInternal(const UChar* characters, unsigned length, DateComponents* out) const
{
    ASSERT(out);
    unsigned end;
    return out->parseDateTime(characters, length, 0, end) && end == length;
}

bool DateTimeInputType::setMillisecondToDateComponents(double value, DateComponents* date) const
{
    ASSERT(date);
    return date->setMillisecondsSinceEpochForDateTime(value);
}

bool DateTimeInputType::isDateTimeField() const
{
    return true;
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
// FIXME: It is better to share code for DateTimeInputType::formatDateTimeFieldsState()
// and DateTimeInputLocalType::formatDateTimeFieldsState().
String DateTimeInputType::formatDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState) const
{
    if (!dateTimeFieldsState.hasDayOfMonth() || !dateTimeFieldsState.hasMonth() || !dateTimeFieldsState.hasYear()
        || !dateTimeFieldsState.hasHour() || !dateTimeFieldsState.hasMinute() || !dateTimeFieldsState.hasAMPM())
        return emptyString();

    if (dateTimeFieldsState.hasMillisecond() && dateTimeFieldsState.millisecond()) {
        return String::format("%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
            dateTimeFieldsState.year(),
            dateTimeFieldsState.month(),
            dateTimeFieldsState.dayOfMonth(),
            dateTimeFieldsState.hour23(),
            dateTimeFieldsState.minute(),
            dateTimeFieldsState.hasSecond() ? dateTimeFieldsState.second() : 0,
            dateTimeFieldsState.millisecond());
    }

    if (dateTimeFieldsState.hasSecond() && dateTimeFieldsState.second()) {
        return String::format("%04u-%02u-%02uT%02u:%02u:%02uZ",
            dateTimeFieldsState.year(),
            dateTimeFieldsState.month(),
            dateTimeFieldsState.dayOfMonth(),
            dateTimeFieldsState.hour23(),
            dateTimeFieldsState.minute(),
            dateTimeFieldsState.second());
    }

    return String::format("%04u-%02u-%02uT%02u:%02uZ",
        dateTimeFieldsState.year(),
        dateTimeFieldsState.month(),
        dateTimeFieldsState.dayOfMonth(),
        dateTimeFieldsState.hour23(),
        dateTimeFieldsState.minute());
}

void DateTimeInputType::setupLayoutParameters(DateTimeEditElement::LayoutParameters& layoutParameters, const DateComponents& date) const
{
    if (shouldHaveSecondField(date)) {
        layoutParameters.dateTimeFormat = layoutParameters.locale.dateTimeFormatWithSeconds();
        layoutParameters.fallbackDateTimeFormat = "yyyy-MM-dd'T'HH:mm:ss'Z'";
    } else {
        layoutParameters.dateTimeFormat = layoutParameters.locale.dateTimeFormatWithoutSeconds();
        layoutParameters.fallbackDateTimeFormat = "yyyy-MM-dd'T'HH:mm'Z'";
    }
    if (!parseToDateComponents(element()->fastGetAttribute(minAttr), &layoutParameters.minimum))
        layoutParameters.minimum = DateComponents();
    if (!parseToDateComponents(element()->fastGetAttribute(maxAttr), &layoutParameters.maximum))
        layoutParameters.maximum = DateComponents();
    layoutParameters.placeholderForDay = placeholderForDayOfMonthField();
    layoutParameters.placeholderForMonth = placeholderForMonthField();
    layoutParameters.placeholderForYear = placeholderForYearField();
}

bool DateTimeInputType::isValidFormat(bool hasYear, bool hasMonth, bool hasWeek, bool hasDay, bool hasAMPM, bool hasHour, bool hasMinute, bool hasSecond) const
{
    return hasYear && hasMonth && hasDay && hasAMPM && hasHour && hasMinute;
}
#endif

String DateTimeInputType::sanitizeValue(const String& proposedValue) const
{
    DateComponents date;
    if (!parseToDateComponents(proposedValue, &date))
        return String();
    return date.toString();
}

} // namespace WebCore

#endif
