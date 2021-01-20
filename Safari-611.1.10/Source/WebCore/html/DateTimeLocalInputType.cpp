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

#if ENABLE(INPUT_TYPE_DATETIMELOCAL)

#include "DateComponents.h"
#include "DateTimeFieldsState.h"
#include "Decimal.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputTypeNames.h"
#include "PlatformLocale.h"
#include "StepRange.h"

namespace WebCore {

using namespace HTMLNames;

static const int dateTimeLocalDefaultStep = 60;
static const int dateTimeLocalDefaultStepBase = 0;
static const int dateTimeLocalStepScaleFactor = 1000;
static const StepRange::StepDescription dateTimeLocalStepDescription { dateTimeLocalDefaultStep, dateTimeLocalDefaultStepBase, dateTimeLocalStepScaleFactor, StepRange::ScaledStepValueShouldBeInteger };

const AtomString& DateTimeLocalInputType::formControlType() const
{
    return InputTypeNames::datetimelocal();
}

DateComponentsType DateTimeLocalInputType::dateType() const
{
    return DateComponentsType::DateTimeLocal;
}

double DateTimeLocalInputType::valueAsDate() const
{
    // valueAsDate doesn't work for the datetime-local type according to the standard.
    return DateComponents::invalidMilliseconds();
}

ExceptionOr<void> DateTimeLocalInputType::setValueAsDate(double value) const
{
    // valueAsDate doesn't work for the datetime-local type according to the standard.
    return InputType::setValueAsDate(value);
}

StepRange DateTimeLocalInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    ASSERT(element());
    const Decimal stepBase = parseToNumber(element()->attributeWithoutSynchronization(minAttr), 0);
    const Decimal minimum = parseToNumber(element()->attributeWithoutSynchronization(minAttr), Decimal::fromDouble(DateComponents::minimumDateTime()));
    const Decimal maximum = parseToNumber(element()->attributeWithoutSynchronization(maxAttr), Decimal::fromDouble(DateComponents::maximumDateTime()));
    const Decimal step = StepRange::parseStep(anyStepHandling, dateTimeLocalStepDescription, element()->attributeWithoutSynchronization(stepAttr));
    return StepRange(stepBase, RangeLimitations::Valid, minimum, maximum, step, dateTimeLocalStepDescription);
}

Optional<DateComponents> DateTimeLocalInputType::parseToDateComponents(const StringView& source) const
{
    return DateComponents::fromParsingDateTimeLocal(source);
}

Optional<DateComponents> DateTimeLocalInputType::setMillisecondToDateComponents(double value) const
{
    return DateComponents::fromMillisecondsSinceEpochForDateTimeLocal(value);
}

bool DateTimeLocalInputType::isDateTimeLocalField() const
{
    return true;
}

bool DateTimeLocalInputType::isValidFormat(OptionSet<DateTimeFormatValidationResults> results) const
{
    return results.containsAll({ DateTimeFormatValidationResults::HasYear, DateTimeFormatValidationResults::HasMonth, DateTimeFormatValidationResults::HasDay, DateTimeFormatValidationResults::HasHour, DateTimeFormatValidationResults::HasMinute, DateTimeFormatValidationResults::HasMeridiem });
}

String DateTimeLocalInputType::formatDateTimeFieldsState(const DateTimeFieldsState& state) const
{
    if (!state.year || !state.month || !state.dayOfMonth || !state.hour || !state.minute || !state.meridiem)
        return emptyString();

    auto dateString = makeString(pad('0', 4, *state.year), '-', pad('0', 2, *state.month), '-', pad('0', 2, *state.dayOfMonth));
    auto hourMinuteString = makeString(pad('0', 2, *state.hour23()), ':', pad('0', 2, *state.minute));

    if (state.millisecond)
        return makeString(dateString, 'T', hourMinuteString, ':', pad('0', 2, state.second ? *state.second : 0), '.', pad('0', 3, *state.millisecond));

    if (state.second)
        return makeString(dateString, 'T', hourMinuteString, ':', pad('0', 2, *state.second));

    return makeString(dateString, 'T', hourMinuteString);
}

void DateTimeLocalInputType::setupLayoutParameters(DateTimeEditElement::LayoutParameters& layoutParameters, const DateComponents& date) const
{
    layoutParameters.shouldHaveMillisecondField = shouldHaveMillisecondField(date);

    if (layoutParameters.shouldHaveMillisecondField || shouldHaveSecondField(date)) {
        layoutParameters.dateTimeFormat = layoutParameters.locale.dateTimeFormatWithSeconds();
        layoutParameters.fallbackDateTimeFormat = "yyyy-MM-dd'T'HH:mm:ss"_s;
    } else {
        layoutParameters.dateTimeFormat = layoutParameters.locale.dateTimeFormatWithoutSeconds();
        layoutParameters.fallbackDateTimeFormat = "yyyy-MM-dd'T'HH:mm"_s;
    }
}

} // namespace WebCore

#endif
