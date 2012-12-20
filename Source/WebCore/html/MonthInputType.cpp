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
#include "MonthInputType.h"

#include "DateComponents.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputTypeNames.h"
#include <wtf/CurrentTime.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/PassOwnPtr.h>

#if ENABLE(INPUT_TYPE_MONTH)

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeFieldsState.h"
#include "LocalizedStrings.h"
#include "PlatformLocale.h"
#include <wtf/text/WTFString.h>
#endif

namespace WebCore {

using namespace HTMLNames;

static const int monthDefaultStep = 1;
static const int monthDefaultStepBase = 0;
static const int monthStepScaleFactor = 1;

PassOwnPtr<InputType> MonthInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new MonthInputType(element));
}

const AtomicString& MonthInputType::formControlType() const
{
    return InputTypeNames::month();
}

DateComponents::Type MonthInputType::dateType() const
{
    return DateComponents::Month;
}

double MonthInputType::valueAsDate() const
{
    DateComponents date;
    if (!parseToDateComponents(element()->value(), &date))
        return DateComponents::invalidMilliseconds();
    double msec = date.millisecondsSinceEpoch();
    ASSERT(isfinite(msec));
    return msec;
}

String MonthInputType::serializeWithMilliseconds(double value) const
{
    DateComponents date;
    if (!date.setMillisecondsSinceEpochForMonth(value))
        return String();
    return serializeWithComponents(date);
}

Decimal MonthInputType::defaultValueForStepUp() const
{
    double current = currentTimeMS();
    double utcOffset = calculateUTCOffset();
    double dstOffset = calculateDSTOffset(current, utcOffset);
    int offset = static_cast<int>((utcOffset + dstOffset) / msPerMinute);
    current += offset * msPerMinute;

    DateComponents date;
    date.setMillisecondsSinceEpochForMonth(current);
    double months = date.monthsSinceEpoch();
    ASSERT(isfinite(months));
    return Decimal::fromDouble(months);
}

StepRange MonthInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    DEFINE_STATIC_LOCAL(const StepRange::StepDescription, stepDescription, (monthDefaultStep, monthDefaultStepBase, monthStepScaleFactor, StepRange::ParsedStepValueShouldBeInteger));

    const Decimal stepBase = parseToNumber(element()->fastGetAttribute(minAttr), Decimal::fromDouble(monthDefaultStepBase));
    const Decimal minimum = parseToNumber(element()->fastGetAttribute(minAttr), Decimal::fromDouble(DateComponents::minimumMonth()));
    const Decimal maximum = parseToNumber(element()->fastGetAttribute(maxAttr), Decimal::fromDouble(DateComponents::maximumMonth()));
    const Decimal step = StepRange::parseStep(anyStepHandling, stepDescription, element()->fastGetAttribute(stepAttr));
    return StepRange(stepBase, minimum, maximum, step, stepDescription);
}

Decimal MonthInputType::parseToNumber(const String& src, const Decimal& defaultValue) const
{
    DateComponents date;
    if (!parseToDateComponents(src, &date))
        return defaultValue;
    double months = date.monthsSinceEpoch();
    ASSERT(isfinite(months));
    return Decimal::fromDouble(months);
}

bool MonthInputType::parseToDateComponentsInternal(const UChar* characters, unsigned length, DateComponents* out) const
{
    ASSERT(out);
    unsigned end;
    return out->parseMonth(characters, length, 0, end) && end == length;
}

bool MonthInputType::setMillisecondToDateComponents(double value, DateComponents* date) const
{
    ASSERT(date);
    return date->setMonthsSinceEpoch(value);
}

bool MonthInputType::isMonthField() const
{
    return true;
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
String MonthInputType::formatDateTimeFieldsState(const DateTimeFieldsState& dateTimeFieldsState) const
{
    if (!dateTimeFieldsState.hasMonth() || !dateTimeFieldsState.hasYear())
        return emptyString();
    return String::format("%04u-%02u", dateTimeFieldsState.year(), dateTimeFieldsState.month());
}

void MonthInputType::setupLayoutParameters(DateTimeEditElement::LayoutParameters& layoutParameters, const DateComponents& date) const
{
    layoutParameters.dateTimeFormat = layoutParameters.locale.monthFormat();
    layoutParameters.fallbackDateTimeFormat = "MM/yyyy";
    if (!parseToDateComponents(element()->fastGetAttribute(minAttr), &layoutParameters.minimum))
        layoutParameters.minimum = DateComponents();
    if (!parseToDateComponents(element()->fastGetAttribute(maxAttr), &layoutParameters.maximum))
        layoutParameters.maximum = DateComponents();
    layoutParameters.placeholderForMonth = "--";
    layoutParameters.placeholderForYear = "----";
}
#endif
} // namespace WebCore

#endif
