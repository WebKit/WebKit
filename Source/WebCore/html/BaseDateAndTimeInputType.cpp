/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "BaseDateAndTimeInputType.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "PlatformLocale.h"
#include <limits>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringView.h>

namespace WebCore {

using namespace HTMLNames;

static const int msecPerMinute = 60 * 1000;
static const int msecPerSecond = 1000;

double BaseDateAndTimeInputType::valueAsDate() const
{
    return valueAsDouble();
}

ExceptionOr<void> BaseDateAndTimeInputType::setValueAsDate(double value) const
{
    ASSERT(element());
    element()->setValue(serializeWithMilliseconds(value));
    return { };
}

double BaseDateAndTimeInputType::valueAsDouble() const
{
    ASSERT(element());
    const Decimal value = parseToNumber(element()->value(), Decimal::nan());
    return value.isFinite() ? value.toDouble() : DateComponents::invalidMilliseconds();
}

ExceptionOr<void> BaseDateAndTimeInputType::setValueAsDecimal(const Decimal& newValue, TextFieldEventBehavior eventBehavior) const
{
    ASSERT(element());
    element()->setValue(serialize(newValue), eventBehavior);
    return { };
}

bool BaseDateAndTimeInputType::typeMismatchFor(const String& value) const
{
    return !value.isEmpty() && parseToDateComponents(value) == WTF::nullopt;
}

bool BaseDateAndTimeInputType::typeMismatch() const
{
    ASSERT(element());
    return typeMismatchFor(element()->value());
}

Decimal BaseDateAndTimeInputType::defaultValueForStepUp() const
{
    double ms = WallTime::now().secondsSinceEpoch().milliseconds();
    int offset = calculateLocalTimeOffset(ms).offset / msPerMinute;
    return Decimal::fromDouble(ms + (offset * msPerMinute));
}

bool BaseDateAndTimeInputType::isSteppable() const
{
    return true;
}

void BaseDateAndTimeInputType::attributeChanged(const QualifiedName& name)
{
    if (name == maxAttr || name == minAttr) {
        if (auto* element = this->element())
            element->invalidateStyleForSubtree();
    }
    InputType::attributeChanged(name);
}

Decimal BaseDateAndTimeInputType::parseToNumber(const String& source, const Decimal& defaultValue) const
{
    auto date = parseToDateComponents(source);
    if (!date)
        return defaultValue;
    double msec = date->millisecondsSinceEpoch();
    ASSERT(std::isfinite(msec));
    return Decimal::fromDouble(msec);
}

String BaseDateAndTimeInputType::serialize(const Decimal& value) const
{
    if (!value.isFinite())
        return { };
    auto date = setMillisecondToDateComponents(value.toDouble());
    if (!date)
        return { };
    return serializeWithComponents(*date);
}

String BaseDateAndTimeInputType::serializeWithComponents(const DateComponents& date) const
{
    ASSERT(element());
    Decimal step;
    if (!element()->getAllowedValueStep(&step))
        return date.toString();
    if (step.remainder(msecPerMinute).isZero())
        return date.toString(DateComponents::None);
    if (step.remainder(msecPerSecond).isZero())
        return date.toString(DateComponents::Second);
    return date.toString(DateComponents::Millisecond);
}

String BaseDateAndTimeInputType::serializeWithMilliseconds(double value) const
{
    return serialize(Decimal::fromDouble(value));
}

String BaseDateAndTimeInputType::localizeValue(const String& proposedValue) const
{
    auto date = parseToDateComponents(proposedValue);
    if (!date)
        return proposedValue;

    ASSERT(element());
    String localized = element()->locale().formatDateTime(*date);
    return localized.isEmpty() ? proposedValue : localized;
}

String BaseDateAndTimeInputType::visibleValue() const
{
    ASSERT(element());
    return localizeValue(element()->value());
}

String BaseDateAndTimeInputType::sanitizeValue(const String& proposedValue) const
{
    return typeMismatchFor(proposedValue) ? String() : proposedValue;
}

bool BaseDateAndTimeInputType::supportsReadOnly() const
{
    return true;
}

bool BaseDateAndTimeInputType::shouldRespectListAttribute()
{
    return InputType::themeSupportsDataListUI(this);
}

bool BaseDateAndTimeInputType::valueMissing(const String& value) const
{
    ASSERT(element());
    return element()->isRequired() && value.isEmpty();
}

#if PLATFORM(IOS_FAMILY)
bool BaseDateAndTimeInputType::isKeyboardFocusable(KeyboardEvent*) const
{
    ASSERT(element());
    return !element()->isReadOnly() && element()->isTextFormControlFocusable();
}
#endif

} // namespace WebCore
#endif
