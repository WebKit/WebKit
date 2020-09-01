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
#if ENABLE(INPUT_TYPE_MONTH)
#include "MonthInputType.h"

#include "DateComponents.h"
#include "Decimal.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputTypeNames.h"
#include "StepRange.h"
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace HTMLNames;

static const int monthDefaultStep = 1;
static const int monthDefaultStepBase = 0;
static const int monthStepScaleFactor = 1;
static const StepRange::StepDescription monthStepDescription { monthDefaultStep, monthDefaultStepBase, monthStepScaleFactor, StepRange::ParsedStepValueShouldBeInteger };

const AtomString& MonthInputType::formControlType() const
{
    return InputTypeNames::month();
}

DateComponentsType MonthInputType::dateType() const
{
    return DateComponentsType::Month;
}

double MonthInputType::valueAsDate() const
{
    ASSERT(element());
    auto date = parseToDateComponents(element()->value());
    if (!date)
        return DateComponents::invalidMilliseconds();
    double msec = date->millisecondsSinceEpoch();
    ASSERT(std::isfinite(msec));
    return msec;
}

String MonthInputType::serializeWithMilliseconds(double value) const
{
    auto date = DateComponents::fromMillisecondsSinceEpochForMonth(value);
    if (!date)
        return { };
    return serializeWithComponents(*date);
}

Decimal MonthInputType::defaultValueForStepUp() const
{
    double current = WallTime::now().secondsSinceEpoch().milliseconds();
    int offset = calculateLocalTimeOffset(current).offset / msPerMinute;
    current += offset * msPerMinute;

    auto date = DateComponents::fromMillisecondsSinceEpochForMonth(current);
    if (!date)
        return  { };

    double months = date->monthsSinceEpoch();
    ASSERT(std::isfinite(months));
    return Decimal::fromDouble(months);
}

StepRange MonthInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    ASSERT(element());
    const Decimal stepBase = parseToNumber(element()->attributeWithoutSynchronization(minAttr), Decimal::fromDouble(monthDefaultStepBase));
    const Decimal minimum = parseToNumber(element()->attributeWithoutSynchronization(minAttr), Decimal::fromDouble(DateComponents::minimumMonth()));
    const Decimal maximum = parseToNumber(element()->attributeWithoutSynchronization(maxAttr), Decimal::fromDouble(DateComponents::maximumMonth()));
    const Decimal step = StepRange::parseStep(anyStepHandling, monthStepDescription, element()->attributeWithoutSynchronization(stepAttr));
    return StepRange(stepBase, RangeLimitations::Valid, minimum, maximum, step, monthStepDescription);
}

Decimal MonthInputType::parseToNumber(const String& src, const Decimal& defaultValue) const
{
    auto date = parseToDateComponents(src);
    if (!date)
        return defaultValue;
    double months = date->monthsSinceEpoch();
    ASSERT(std::isfinite(months));
    return Decimal::fromDouble(months);
}

Optional<DateComponents> MonthInputType::parseToDateComponents(const StringView& source) const
{
    return DateComponents::fromParsingMonth(source);
}

Optional<DateComponents> MonthInputType::setMillisecondToDateComponents(double value) const
{
    return DateComponents::fromMonthsSinceEpoch(value);
}

bool MonthInputType::isMonthField() const
{
    return true;
}

void MonthInputType::handleDOMActivateEvent(Event&)
{
}

bool MonthInputType::isValidFormat(OptionSet<DateTimeFormatValidationResults> results) const
{
    return results.containsAll({ DateTimeFormatValidationResults::HasYear, DateTimeFormatValidationResults::HasMonth });
}

String MonthInputType::formatDateTimeFieldsState(const DateTimeFieldsState&) const
{
    return emptyString();
}

void MonthInputType::setupLayoutParameters(DateTimeEditElement::LayoutParameters&) const
{
}

} // namespace WebCore

#endif
