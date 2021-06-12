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
#if ENABLE(INPUT_TYPE_TIME)
#include "TimeInputType.h"

#include "DateComponents.h"
#include "DateTimeFieldsState.h"
#include "Decimal.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputTypeNames.h"
#include "PlatformLocale.h"
#include "StepRange.h"
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>

namespace WebCore {

using namespace HTMLNames;

static const int timeDefaultStep = 60;
static const int timeDefaultStepBase = 0;
static const int timeStepScaleFactor = 1000;
static const StepRange::StepDescription timeStepDescription { timeDefaultStep, timeDefaultStepBase, timeStepScaleFactor, StepRange::ScaledStepValueShouldBeInteger };

TimeInputType::TimeInputType(HTMLInputElement& element)
    : BaseDateAndTimeInputType(Type::Time, element)
{
}

const AtomString& TimeInputType::formControlType() const
{
    return InputTypeNames::time();
}

DateComponentsType TimeInputType::dateType() const
{
    return DateComponentsType::Time;
}

Decimal TimeInputType::defaultValueForStepUp() const
{
    double current = WallTime::now().secondsSinceEpoch().milliseconds();
    int offset = calculateLocalTimeOffset(current).offset / msPerMinute;
    current += offset * msPerMinute;

    auto date = DateComponents::fromMillisecondsSinceMidnight(current);
    if (!date)
        return  { };

    double milliseconds = date->millisecondsSinceEpoch();
    ASSERT(std::isfinite(milliseconds));
    return Decimal::fromDouble(milliseconds);
}

StepRange TimeInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    ASSERT(element());
    const Decimal stepBase = parseToNumber(element()->attributeWithoutSynchronization(minAttr), 0);
    const Decimal minimum = parseToNumber(element()->attributeWithoutSynchronization(minAttr), Decimal::fromDouble(DateComponents::minimumTime()));
    const Decimal maximum = parseToNumber(element()->attributeWithoutSynchronization(maxAttr), Decimal::fromDouble(DateComponents::maximumTime()));
    const Decimal step = StepRange::parseStep(anyStepHandling, timeStepDescription, element()->attributeWithoutSynchronization(stepAttr));
    return StepRange(stepBase, RangeLimitations::Valid, minimum, maximum, step, timeStepDescription);
}

std::optional<DateComponents> TimeInputType::parseToDateComponents(const StringView& source) const
{
    return DateComponents::fromParsingTime(source);
}

std::optional<DateComponents> TimeInputType::setMillisecondToDateComponents(double value) const
{
    return DateComponents::fromMillisecondsSinceMidnight(value);
}

void TimeInputType::handleDOMActivateEvent(Event&)
{
}

bool TimeInputType::isValidFormat(OptionSet<DateTimeFormatValidationResults> results) const
{
    return results.containsAll({ DateTimeFormatValidationResults::HasHour, DateTimeFormatValidationResults::HasMinute, DateTimeFormatValidationResults::HasMeridiem });
}

String TimeInputType::formatDateTimeFieldsState(const DateTimeFieldsState& state) const
{
    if (!state.hour || !state.minute || !state.meridiem)
        return emptyString();

    auto hourMinuteString = makeString(pad('0', 2, *state.hour23()), ':', pad('0', 2, *state.minute));

    if (state.millisecond)
        return makeString(hourMinuteString, ':', pad('0', 2, state.second ? *state.second : 0), '.', pad('0', 3, *state.millisecond));

    if (state.second)
        return makeString(hourMinuteString, ':', pad('0', 2, *state.second));

    return hourMinuteString;
}

void TimeInputType::setupLayoutParameters(DateTimeEditElement::LayoutParameters& layoutParameters, const DateComponents& date) const
{
    layoutParameters.shouldHaveMillisecondField = shouldHaveMillisecondField(date);

    if (layoutParameters.shouldHaveMillisecondField || shouldHaveSecondField(date)) {
        layoutParameters.dateTimeFormat = layoutParameters.locale.timeFormat();
        layoutParameters.fallbackDateTimeFormat = "HH:mm:ss"_s;
    } else {
        layoutParameters.dateTimeFormat = layoutParameters.locale.shortTimeFormat();
        layoutParameters.fallbackDateTimeFormat = "HH:mm"_s;
    }
}

} // namespace WebCore

#endif
