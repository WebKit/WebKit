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
#if ENABLE(INPUT_TYPE_DATE)
#include "DateInputType.h"

#include "DateComponents.h"
#include "DateTimeFieldsState.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputTypeNames.h"
#include "PlatformLocale.h"
#include "StepRange.h"

namespace WebCore {

using namespace HTMLNames;

static const int dateDefaultStep = 1;
static const int dateDefaultStepBase = 0;
static const int dateStepScaleFactor = 86400000;
static const StepRange::StepDescription dateStepDescription { dateDefaultStep, dateDefaultStepBase, dateStepScaleFactor, StepRange::ParsedStepValueShouldBeInteger };

DateInputType::DateInputType(HTMLInputElement& element)
    : BaseDateAndTimeInputType(Type::Date, element)
{
}

const AtomString& DateInputType::formControlType() const
{
    return InputTypeNames::date();
}

DateComponentsType DateInputType::dateType() const
{
    return DateComponentsType::Date;
}

StepRange DateInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    ASSERT(element());
    const Decimal stepBase = parseToNumber(element()->attributeWithoutSynchronization(minAttr), 0);
    const Decimal minimum = parseToNumber(element()->attributeWithoutSynchronization(minAttr), Decimal::fromDouble(DateComponents::minimumDate()));
    const Decimal maximum = parseToNumber(element()->attributeWithoutSynchronization(maxAttr), Decimal::fromDouble(DateComponents::maximumDate()));
    const Decimal step = StepRange::parseStep(anyStepHandling, dateStepDescription, element()->attributeWithoutSynchronization(stepAttr));
    return StepRange(stepBase, RangeLimitations::Valid, minimum, maximum, step, dateStepDescription);
}

std::optional<DateComponents> DateInputType::parseToDateComponents(const StringView& source) const
{
    return DateComponents::fromParsingDate(source);
}

std::optional<DateComponents> DateInputType::setMillisecondToDateComponents(double value) const
{
    return DateComponents::fromMillisecondsSinceEpochForDate(value);
}

bool DateInputType::isValidFormat(OptionSet<DateTimeFormatValidationResults> results) const
{
    return results.containsAll({ DateTimeFormatValidationResults::HasYear, DateTimeFormatValidationResults::HasMonth, DateTimeFormatValidationResults::HasDay });
}

String DateInputType::formatDateTimeFieldsState(const DateTimeFieldsState& state) const
{
    if (!state.dayOfMonth || !state.month || !state.year)
        return emptyString();

    return makeString(pad('0', 4, *state.year), '-', pad('0', 2, *state.month), '-', pad('0', 2, *state.dayOfMonth));
}

void DateInputType::setupLayoutParameters(DateTimeEditElement::LayoutParameters& layoutParameters, const DateComponents&) const
{
    layoutParameters.dateTimeFormat = layoutParameters.locale.dateFormat();
    layoutParameters.fallbackDateTimeFormat = "yyyy-MM-dd"_s;
}

} // namespace WebCore
#endif
